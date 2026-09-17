/*
 * ani-switch entry point
 *
 * Adapted from xfangfang/wiliwili (GPL-3.0)
 * SPDX-License-Identifier: AGPL-3.0
 */

#include <borealis.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>

#include "utils/config_helper.hpp"
#include "utils/sqlite_store.hpp"
#include "utils/update_checker.hpp"
#include "ui/theme.hpp"
#include "ui/token_refresh_task.hpp"
#include "net/bgm_auth.hpp"
#include "net/bgm_client.hpp"
#include "net/myani_client.hpp"
#include "player/mpv_core.hpp"
#include "core/episode_resolver.hpp"
#include "core/http_source.hpp"
#include "core/web_selector_provider.hpp"
#include <filesystem>
#include "utils/activity_helper.hpp"
#include "utils/version_helper.hpp"
#include "player/mpv_core.hpp"
#include "player/danmaku_core.hpp"
#include "ui/register_helper.hpp"

#if defined(__SWITCH__)
extern "C" void aniswitchStartupLog(const char* message);
extern "C" int aniswitchStartupReady(void);
#endif

namespace aniswitch {

namespace {

void startupStage(const char* message) {
#if defined(__SWITCH__) && defined(ANISWITCH_SWITCH_DEBUG)
    aniswitchStartupLog(message);
#else
    (void)message;
#endif
}

}  // namespace

int main(int argc, char* argv[]) {
    startupStage("main: entered");

#if defined(__SWITCH__)
    if (!aniswitchStartupReady()) {
        startupStage("main: libnx bootstrap failed");
        return EXIT_FAILURE;
    }
#endif

    // ---- Parse command line flags ------------------------------------------
    startupStage("main: parsing arguments");
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-d") == 0) {
            brls::Logger::setLogLevel(brls::LogLevel::LOG_DEBUG);
        } else if (std::strcmp(argv[i], "-v") == 0) {
            brls::Application::enableDebuggingView(true);
        } else if (std::strcmp(argv[i], "-t") == 0) {
            MPVCore::TERMINAL = true;
        } else if (std::strcmp(argv[i], "-o") == 0) {
            const char* path = (i + 1 < argc) ? argv[++i] : "ani-switch.log";
            std::FILE* output = std::fopen(path, "w+");
            if (output != nullptr) {
                brls::Logger::setLogOutput(output);
            } else {
                startupStage("main: requested log file could not be opened");
            }
        }
    }
    startupStage("main: arguments parsed");

    // ---- Init app + i18n ---------------------------------------------------
    startupStage("main: Application::init begin");
    try {
        if (!brls::Application::init()) {
            startupStage("main: Application::init returned false");
            brls::Logger::error("Unable to init application");
            return EXIT_FAILURE;
        }
    } catch (const std::exception& e) {
        startupStage("main: Application::init threw");
        brls::Logger::error("Unable to init application: {}", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        startupStage("main: Application::init threw unknown exception");
        return EXIT_FAILURE;
    }
    startupStage("main: Application::init done");

    // Install ani-switch's dark- and light-theme palette extensions
    // (ani_accent, ani_card_*, ani_text_*, ani_tag_*) on top of
    // borealis's stock themes so individual fragments can pull a
    // single token instead of repeating nvgRGB literals.  v17.0
    // also installs the light palette and exposes a runtime
    // switcher (aniswitch::theme::applyTheme), wired up below
    // after the config loads.
    aniswitch::theme::registerDarkTheme();
    aniswitch::theme::registerLightTheme();

    // Load settings only after Borealis has initialized its platform/logger.
    startupStage("main: config init begin");
    try {
        ProgramConfig::instance().init();
    } catch (const std::exception& e) {
        brls::Logger::error("Unable to load configuration: {}", e.what());
    } catch (...) {
        brls::Logger::error("Unable to load configuration: unknown exception");
    }
    // v17.0: apply the user's saved theme (Auto / Light / Dark)
    // before the first activity is created so the entire UI
    // tree paints in the chosen palette on first frame.
    {
        auto& cfg = ProgramConfig::instance();
        const int themeIdx = cfg.getSettingItem<int>(
            SettingItem::APP_THEME, /*Auto=*/0);
        aniswitch::theme::applyTheme(
            aniswitch::theme::themeChoiceFromIndex(themeIdx));
    }
    // Feed the configured Bangumi access token into both the Bangumi
    // REST client and the myani danmaku client.  myani re-exchanges it
    // for a per-session token on first use; BangumiClient reuses it
    // directly.
    {
        const std::string pat = ProgramConfig::instance().getBangumiAccessToken();
        if (!pat.empty()) {
            aniswitch::BangumiClient::setAccessToken(pat);
            aniswitch::MyaniClient::instance().setBangumiToken(pat);
            startupStage("main: bangumi token wired");
        }
    }
    startupStage("main: config init done");

    // Periodic Bangumi token refresh (v16.10).
    //
    // v15.3 / v16.9 used std::thread + detach + std::this_thread::sleep_for
    // to refresh the access token every hour.  On Switch newlib that
    // pattern std::terminates the NRO — the v15.3 outer+inner try/catch
    // didn't help because no exception is raised, the process is just
    // killed (probably pthread limit / std::chrono in newlib, but the
    // exact cause is opaque from startup.log).
    //
    // v16.10 replaces the detached thread with brls::RepeatingTask,
    // which piggybacks on the borealis main-loop timer list.  Each
    // tick is dispatched on the UI thread, no extra thread is created,
    // and a thrown exception is contained inside the task's run()
    // try/catch instead of taking the NRO down.  The token refresh
    // is only meaningful when we have a refresh token, so we leave
    // the task inert in the no-token case.
    //
    // We new the instance now (after Application::init so the
    // RepeatingTask's Timer can be safely registered) and start it
    // after createWindow so the first tick is never racy with the
    // main-loop bootstrap.
    static auto* g_tokenRefreshTask =
        new aniswitch::TokenRefreshTask();  // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    startupStage("main: token refresh task constructed");

    // On Switch, the applet focus state is wired to brls's
    // WindowFocusChangedEvent by borealis (see
    // third_party/borealis/library/lib/platforms/switch/switch_platform.cpp).
    // We subscribe here so playback pauses when the user presses the
    // HOME button or the console goes to sleep.  Resuming is left to
    // the user — pressing the X button on the player activity is
    // the resume gesture and it must remain a deliberate user
    // action (auto-resume could play a paused scene at full volume
    // if the user expected silence).
    startupStage("main: focus event subscribe begin");
    brls::Application::getWindowFocusChangedEvent()->subscribe(
        [](bool inFocus) {
            if (inFocus) {
                brls::Logger::debug("app: gained focus");
                return;
            }
            if (MPVCore::instance().isPlaying() && !MPVCore::instance().isPaused()) {
                MPVCore::instance().pause();
                brls::Logger::info("app: lost focus, paused playback");
            }
        });
    startupStage("main: focus event subscribed");

    // On Switch, exit to home menu when app closes
    startupStage("main: exitToHomeMode begin");
    if (brls::Application::getPlatform()->getName() == "Switch") {
        brls::Application::getPlatform()->exitToHomeMode(true);
    }
    startupStage("main: exitToHomeMode set");

    startupStage("main: createWindow begin");
    try {
        brls::Application::createWindow("ani-switch");
    } catch (const std::exception& e) {
        startupStage("main: createWindow threw");
        brls::Logger::error("Unable to create window: {}", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        startupStage("main: createWindow threw unknown exception");
        return EXIT_FAILURE;
    }
    startupStage("main: createWindow done");

    // Now that the main loop is up, kick off the periodic token
    // refresh.  The task only does real work when we have a refresh
    // token; otherwise its run() is a no-op that returns immediately.
    if (!ProgramConfig::instance().getBangumiRefreshToken().empty()) {
        g_tokenRefreshTask->start();
        startupStage("main: token refresh task started");
    } else {
        startupStage("main: no refresh token, skip token task");
    }

    // In-app update check.
    //
    // v16.9 had this fire a `std::thread([](){ cpr::Get(...) }).detach()`
    // (see update_checker.cpp) which is the same landmine as the
    // v11-v16.9 token refresh thread — on Switch newlib the detached
    // std::thread std::terminates the whole NRO (no exception is
    // raised, the process is just killed).  v16.10 ships the check
    // DISABLED; we'll re-enable it through a brls::RepeatingTask in
    // v16.11 once the main-loop-driven task pattern is validated
    // end-to-end.  The API surface in update_checker.hpp is left
    // intact so the call site is one line to re-enable.
    startupStage("main: update check skipped (v16.10)");

    bool databaseReady = false;
    startupStage("main: database open begin");
    try {
        std::filesystem::create_directories(ProgramConfig::instance().getConfigDir());
        databaseReady = SQLiteStore::instance().open(ProgramConfig::instance().getConfigDir() + "/ani-switch.store.json");
        startupStage(databaseReady ? "main: database open ok" : "main: database open failed");
    } catch (const std::exception& e) {
        startupStage(e.what());
        startupStage("main: database open threw");
    }
    if (!databaseReady) startupStage("main: database open failed (continuing)");
    startupStage("main: web selector init");
    auto webSelector = std::make_shared<WebSelectorProvider>();
    SourceManager::instance().registerProvider(webSelector);
    EpisodeResolver::setWebSelectorProvider(webSelector.get());
    SourceManager::instance().registerProvider(std::make_shared<HTTPSourceProvider>());
    startupStage("main: web selector init done");

    // Register custom views / themes / styles
    startupStage("main: registering UI begin");
    try {
        aniswitch::Register::initCustomView();
        startupStage("main: registering UI view done");
        aniswitch::Register::initCustomTheme();
        startupStage("main: registering UI theme done");
        aniswitch::Register::initCustomStyle();
        startupStage("main: registering UI style done");
    } catch (const std::exception& e) {
        startupStage("main: registering UI threw");
        startupStage(e.what());
    } catch (...) {
        startupStage("main: registering UI threw unknown");
    }

    brls::Application::getPlatform()->disableScreenDimming(false);
    startupStage("main: disableScreenDimming set");

    // ---- Route to the appropriate entry screen -----------------------------
    startupStage("main: opening initial activity begin");
    try {
        if (!databaseReady) {
            auto* box = new brls::Box();
            box->setAxis(brls::Axis::COLUMN);
            box->setPadding(30);
            auto* message = new brls::Label();
            message->setText("数据库初始化失败。\n详细错误已写入 startup.log；数据文件未被删除。\n请保留日志后退出。");
            message->setSingleLine(false);
            box->addView(message);
            auto* close = new brls::Button();
            close->setText("退出");
            close->registerClickAction([](brls::View*) { brls::Application::quit(); return true; });
            box->addView(close);
            brls::Application::pushActivity(new brls::Activity(box));
        } else if (brls::Application::getPlatform()->isApplicationMode()) {
            startupStage("main: route main activity");
            // v17.3: first-run gate.  A fresh install (or one
            // where the user explicitly reset the gate from
            // Settings → "重新走引导") routes through the 5-step
            // OnboardingActivity first; otherwise we go straight
            // to the existing MainActivity.
            if (ProgramConfig::instance().isFirstRun()) {
                startupStage("main: route onboarding (first run)");
                aniswitch::Intent::openOnboarding();
            } else {
                startupStage("main: route main activity");
                aniswitch::Intent::openMain();
            }
        } else {
            startupStage("main: route hint activity");
            aniswitch::Intent::openHint();
        }
    } catch (const std::exception& e) {
        startupStage("main: initial activity threw");
        startupStage(e.what());
        brls::Logger::error("Unable to open initial activity: {}", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        startupStage("main: initial activity threw unknown exception");
        startupStage("main: initial activity exception detail unavailable");
        return EXIT_FAILURE;
    }
    startupStage("main: initial activity opened");

    // ---- Run main loop -----------------------------------------------------
    startupStage("main: entering main loop");
    try {
        while (brls::Application::mainLoop()) {
            // borealis dispatches events, draws UI, renders video, etc.
        }
    } catch (const std::exception& e) {
        startupStage("main: main loop threw");
        startupStage(e.what());
        brls::Logger::error("Main loop threw: {}", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        startupStage("main: main loop threw unknown exception");
        return EXIT_FAILURE;
    }
    startupStage("main: main loop done");

    // ---- Cleanup -----------------------------------------------------------
    ProgramConfig::instance().exit(argv);

    return EXIT_SUCCESS;
}

}  // namespace aniswitch

// C entry point (wiliwili also supports WINRT; we just call into C++ main)
int main(int argc, char* argv[]) {
    return aniswitch::main(argc, argv);
}
