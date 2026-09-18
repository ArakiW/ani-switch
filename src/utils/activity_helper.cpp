// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)

#include "utils/activity_helper.hpp"
#include "utils/config_helper.hpp"
#include "net/bgm_auth.hpp"
#include "net/bgm_client.hpp"
#include <borealis.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/core/thread.hpp>
#include <fmt/format.h>

#if defined(__SWITCH__)
extern "C" void aniswitchStartupLog(const char* message);
#endif

namespace aniswitch {

bool firstRunLoginCodePromptNeeded() {
    auto& c = ProgramConfig::instance();
    return !c.hasLoginInfo() && !c.isFirstRunCodePromptShown() &&
           !c.isFirstRunCodeSkipped();
}

void promptFirstRunLoginCode(std::function<void()> onDone) {
    if (!firstRunLoginCodePromptNeeded()) {
        if (onDone) onDone();
        return;
    }
    // Mark shown immediately so cancel/B cannot re-nag every launch.
    ProgramConfig::instance().setFirstRunCodePromptShown(true);

    auto finish = [onDone]() {
        if (onDone) onDone();
    };

    // brls::Dialog + ImeManager (not EditTextDialog — swkbd is unreliable).
    auto* dlg = new brls::Dialog(
        "输入登录码\n\nBangumi PAT 或 OAuth code（可跳过）。\n"
        "长码按 PAT 校验；短码会暂存，需在「账号登录」完成 PKCE。");
    dlg->setCancelable(true);
    dlg->addButton("跳过", [finish]() {
        ProgramConfig::instance().setFirstRunCodeSkipped(true);
        finish();
    });
    dlg->addButton("输入登录码", [finish]() {
        auto* ime = brls::Application::getImeManager();
        if (!ime) {
            brls::Application::notify("系统输入法不可用，可稍后在设置登录");
            finish();
            return;
        }
        ime->openForText(
            [finish](std::string code) {
                auto pos = code.find("code=");
                if (pos != std::string::npos) code = code.substr(pos + 5);
                while (!code.empty() && (code.back() == ' ' || code.back() == '\n' ||
                                         code.back() == '\r' || code.back() == '\t'))
                    code.pop_back();
                while (!code.empty() && (code.front() == ' ' || code.front() == '\n' ||
                                         code.front() == '\r' || code.front() == '\t'))
                    code.erase(code.begin());

                if (code.empty()) {
                    ProgramConfig::instance().setFirstRunCodeSkipped(true);
                    finish();
                    return;
                }

                if (code.size() >= 24) {
                    // PAT — same path as login_activity method 3.
                    BangumiAuth::verifyPAT(
                        code,
                        [code](std::string userId) {
                            brls::sync([code, userId]() {
                                auto& cfg = ProgramConfig::instance();
                                cfg.setBangumiToken(code, "", 0, userId);
                                BangumiClient::setAccessToken(code);
                                brls::Application::notify(fmt::format(
                                    "登录码已保存 · user {}", userId));
#if defined(__SWITCH__)
                                {
                                    std::string m =
                                        fmt::format("FIRSTRUN: PAT saved user={}", userId);
                                    aniswitchStartupLog(m.c_str());
                                }
#endif
                            });
                        },
                        [](const std::string& msg, int) {
                            brls::sync([msg]() {
                                brls::Application::notify("登录码无效: " + msg);
                            });
                        });
                } else {
                    ProgramConfig::instance().setPendingFirstRunCode(code);
                    brls::Application::notify(
                        "已记录短码；PKCE 请到「账号登录」生成链接后粘贴");
                }
                finish();
            },
            "输入登录码", "Bangumi PAT 或 OAuth code（可跳过）", 255);
    });
    dlg->open();
}

// Forward declarations: actual activities are in src/ui/activity/*.
namespace ui {
    brls::Activity* make_main_activity();
    brls::Activity* make_onboarding_activity();  // v17.3
    brls::Activity* make_local_video_activity(); // v17.4
    brls::Activity* make_bangumi_sync_activity(); // v18.4
    brls::Activity* make_email_login_activity();  // v18.5
    brls::Activity* make_settings_activity();    // v18.6
    brls::Activity* make_theme_preview_activity(); // v18.7
    brls::Activity* make_log_activity();          // v18.8
    brls::Activity* make_hint_activity();
    brls::Activity* make_setting_activity();
    brls::Activity* make_subject_activity(int32_t subjectId, const std::string& type);
    brls::Activity* make_episode_list_activity(int32_t subjectId, int32_t resumeEpisodeId);
    brls::Activity* make_player_activity(int32_t episodeId, const std::string& danmakuSource,
                                         const std::string& videoSource, int64_t resumePositionMs);
    brls::Activity* make_source_picker_activity(int32_t episodeId, int32_t subjectId,
                                                const std::string& title, bool autoPickFirst);
    brls::Activity* make_my_collection_activity();
    brls::Activity* make_history_activity();
    brls::Activity* make_search_activity(const std::string& query);
    brls::Activity* make_login_activity();
    brls::Activity* make_person_activity(int32_t personId);
}  // namespace ui

void Intent::openMain() {
    if (auto* a = ui::make_main_activity()) {
        brls::Application::pushActivity(a);
    } else {
        brls::Logger::error("Intent::openMain: no main activity registered");
    }
}

void Intent::openOnboarding() {
    if (auto* a = ui::make_onboarding_activity()) {
        brls::Application::pushActivity(a);
    } else {
        // Fallback to main if the onboarding activity is missing in this build.
        openMain();
    }
}

void Intent::openHint() {
    if (auto* a = ui::make_hint_activity()) {
        brls::Application::pushActivity(a);
    } else {
        // Fallback to main if hint activity is missing in this build
        openMain();
    }
}

void Intent::openSetting() {
    if (auto* a = ui::make_setting_activity()) {
        brls::Application::pushActivity(a);
    }
}

void Intent::openSubject(int32_t subjectId, const std::string& type) {
    if (auto* a = ui::make_subject_activity(subjectId, type)) {
        brls::Application::pushActivity(a);
    }
}

void Intent::openEpisodeList(int32_t subjectId, int32_t resumeEpisodeId) {
    if (auto* a = ui::make_episode_list_activity(subjectId, resumeEpisodeId)) {
        brls::Application::pushActivity(a);
    }
}

void Intent::openPlayer(int32_t episodeId, const std::string& danmakuSource,
                        const std::string& videoSource, int64_t resumePositionMs) {
    if (auto* a = ui::make_player_activity(episodeId, danmakuSource, videoSource, resumePositionMs)) {
        brls::Application::pushActivity(a);
    }
}

void Intent::openSourcePicker(int32_t episodeId, int32_t subjectId,
                              const std::string& title, bool autoPickFirst) {
    if (auto* a = ui::make_source_picker_activity(episodeId, subjectId, title,
                                                  autoPickFirst)) {
        brls::Application::pushActivity(a);
    }
}

void Intent::openMyCollection() {
    if (auto* a = ui::make_my_collection_activity()) {
        brls::Application::pushActivity(a);
    }
}

void Intent::openHistory() {
    if (auto* a = ui::make_history_activity()) {
        brls::Application::pushActivity(a);
    }
}

void Intent::openSearch(const std::string& query) {
    if (auto* a = ui::make_search_activity(query)) {
        brls::Application::pushActivity(a);
    }
}

void Intent::openLogin() {
    if (auto* a = ui::make_login_activity()) {
        brls::Application::pushActivity(a);
    }
}

void Intent::openPerson(int32_t personId) {
    if (auto* a = ui::make_person_activity(personId)) {
        brls::Application::pushActivity(a);
    }
}

void Intent::openLocalVideo() {
#if defined(__SWITCH__) && defined(ANISWITCH_SWITCH_DEBUG)
    aniswitchStartupLog("MAIN: openLocalVideo");
#endif
    if (auto* a = ui::make_local_video_activity()) {
        brls::Application::pushActivity(a);
    }
}

void Intent::openBangumiSync() {
    if (auto* a = ui::make_bangumi_sync_activity()) {
        brls::Application::pushActivity(a);
    }
}

void Intent::openSettings() {
    // v20.11: use the legacy flat SettingActivity.  SettingsActivity
    // (4 stacked pages + TabFrame history) native-crashes on device
    // right after MAIN: open 设置 with no C++ exception.
    if (auto* a = ui::make_setting_activity()) {
        brls::Application::pushActivity(a);
    }
}

void Intent::openThemePreview() {
    if (auto* a = ui::make_theme_preview_activity()) {
        brls::Application::pushActivity(a);
    }
}

void Intent::openLog() {
    if (auto* a = ui::make_log_activity()) {
        brls::Application::pushActivity(a);
    }
}

void Intent::openEmailLoginStart() {
    if (auto* a = ui::make_email_login_activity()) {
        brls::Application::pushActivity(a);
    }
}

void Intent::openEmailLoginVerify(const std::string& otpId,
                                 const std::string& email,
                                 bool hasExistingUser) {
    // v18.5: single-activity form, we always start at the
    // email-entry step.  The Verify step is reached internally
    // after sendEmailOtp returns.  This is a thin shim that
    // matches the animeko two-screen API shape so a future
    // split into two activities is mechanical.
    (void)otpId;
    (void)email;
    (void)hasExistingUser;
    if (auto* a = ui::make_email_login_activity()) {
        brls::Application::pushActivity(a);
    }
}

}  // namespace aniswitch
