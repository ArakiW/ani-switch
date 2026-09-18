// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)

#include "utils/config_helper.hpp"
#include "net/bgm_client.hpp"
#include "net/http.hpp"
#include "player/tsvitch_video_view.hpp"
#include "player/tsvitch_svg_image.hpp"
#include "player/tsvitch_video_profile.hpp"
#include "player/tsvitch_video_progress_slider.hpp"
#include "player/tsvitch_hint_label.hpp"
#include "player/mpv_core.hpp"
#include "utils/perf_switch.hpp"
#include <borealis.hpp>
#include <filesystem>
#include <borealis/core/logger.hpp>
#include <fmt/format.h>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <sys/stat.h>

#if defined(__SWITCH__)
extern "C" void aniswitchStartupLog(const char* message);
#endif

namespace aniswitch {

#ifndef DEFAULT_CONFIG_DIR
#  ifdef __SWITCH__
#    define DEFAULT_CONFIG_DIR "."
#  else
#    define DEFAULT_CONFIG_DIR "./config"
#  endif
#endif

std::map<SettingItem, ProgramOption> ProgramConfig::SETTING_MAP = {
    {SettingItem::HIDE_BOTTOM_BAR,        {"hideBottomBar",        {"off", "on"},                                          {0, 1}, 0}},
    {SettingItem::APP_THEME,              {"appTheme",             {"auto", "light", "dark"},                              {0, 1, 2}, 0}},
    {SettingItem::APP_LANG,               {"appLang",              {"auto", "zh-Hans", "zh-Hant", "en", "ja", "ko"},      {0, 1, 2, 3, 4, 5}, 0}},
    {SettingItem::PLAYER_AUTO_PLAY,       {"playerAutoPlay",       {"off", "on"},                                          {0, 1}, 1}},
    {SettingItem::PLAYER_LOW_QUALITY,     {"playerLowQuality",     {"off", "on"},                                          {0, 1}, 0}},
    {SettingItem::PLAYER_HWDEC,           {"playerHwdec",          {"auto", "off", "on"},                                  {0, 1, 2}, 1}},
    {SettingItem::PLAYER_STREAM_MODE,     {"playerStreamMode",     {"seamless", "mpv-direct"},                             {0, 1}, 0}},
    {SettingItem::DANMAKU_SMART_MASK,     {"danmakuSmartMask",     {"off", "on"},                                          {0, 1}, 0}},
    {SettingItem::DANMAKU_ON,             {"danmakuOn",            {"off", "on"},                                          {0, 1}, 1}},
    {SettingItem::DANMAKU_FILTER_LEVEL,   {"danmakuFilterLevel",   {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10"},     {1,2,3,4,5,6,7,8,9,10}, 1}},
    {SettingItem::DANMAKU_STYLE_FONTSIZE, {"danmakuFontSize",      {"15", "22", "30", "37", "45", "50"},                  {15,22,30,37,45,50}, 2}},
    {SettingItem::DANMAKU_STYLE_ALPHA,    {"danmakuAlpha",         {"10", "25", "50", "60", "70", "80", "90", "100"},      {10,25,50,60,70,80,90,100}, 6}},
    {SettingItem::DANMAKU_STYLE_SPEED,    {"danmakuSpeed",         {"50", "75", "100", "125", "150"},                      {50,75,100,125,150}, 2}},
    {SettingItem::OPENCC_ON,              {"openccOn",             {"off", "on"},                                          {0, 1}, 1}},
    {SettingItem::TLS_VERIFY,             {"tlsVerify",            {"on", "off"},                                         {1, 0}, 0}},
    {SettingItem::HTTP_TIMEOUT,           {"httpTimeout",          {"3000", "5000", "10000", "15000", "20000"},            {3000,5000,10000,15000,20000}, 2}},
    {SettingItem::SWAP_INTERVAL,          {"swapInterval",         {"1", "2", "3", "4"},                                   {1,2,3,4}, 0}},
    {SettingItem::LIMITED_FPS,            {"limitedFps",           {"30", "45", "60"},                                     {30,45,60}, 2}},
    {SettingItem::PLAYER_VOLUME,          {"playerVolume",         {"0", "25", "50", "75", "100"},                         {0,25,50,75,100}, 4}},
    {SettingItem::PLAYER_SPEED,           {"playerSpeed",          {"100", "125", "150", "175", "200", "300"},             {100,125,150,175,200,300}, 0}},
    {SettingItem::SCROLL_SPEED,           {"scrollSpeed",          {"slow", "normal", "fast"},                             {0, 1, 2}, 1}},
    {SettingItem::DANMAKU_STYLE_FONT,     {"danmakuFontStyle",     {"stroke", "incline", "shadow", "pure"},                {0, 1, 2, 3}, 0}},
};

std::string Register::customThemeColorHex = "";

ProgramConfig::ProgramConfig() = default;

std::string ProgramConfig::getConfigDir() const {
#ifdef __SWITCH__
    // ani-switch is deployed as `sdmc:/switch/aniswitch/<nro>` and the
    // working directory at process start is the NRO install location
    // (which can be read-only depending on the loader).  Always use an
    // absolute path under sdmc: so the JSON config and the JSON
    // persistent store can be written without tripping EIO on a
    // read-only mount.
    return "sdmc:/switch/aniswitch";
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) return std::string(xdg) + "/ani-switch";
    const char* home = std::getenv("HOME");
    if (home) return std::string(home) + "/.config/ani-switch";
    return "./config";
#endif
}

std::string ProgramConfig::getHomePath() const {
#ifdef __SWITCH__
    return ".";
#else
    const char* home = std::getenv("HOME");
    if (home) return std::string(home);
    return ".";
#endif
}

void ProgramConfig::init() {
    load();
    BangumiClient::setAccessToken(hasLoginInfo() ? bangumiAccessToken : "");
    // v20.0: wire the persisted LAN proxy into every cpr session.
    HTTP::applyProxy(httpProxy);

    // v22.1: wire player statics from settings.  Previously these
    // MPVCore fields were NEVER applied on ani-switch — HARDWARE_DEC
    // stayed at its compile-time `false`, so first player open on
    // real device software-decoded inside deko3d and hung/ crashed
    // (AGENTS.md startup.8 / v20.4+).  TsVitch/wiliwili wire these
    // in ProgramConfig::init; we do the same.
    //
    // Switch policy: force hardware decode ON.  averne/ffmpeg ships
    // Tegra NVDEC and borealis deko3d path expects it.  The user
    // toggle (PLAYER_HWDEC auto/off/on) is ignored on Switch for now
    // because software decode is the known crash path; a future
    // on-device test can re-enable the toggle.
    MPVCore::AUTO_PLAY = getBoolOption(SettingItem::PLAYER_AUTO_PLAY);
    MPVCore::LOW_QUALITY = getBoolOption(SettingItem::PLAYER_LOW_QUALITY);
    // v22.3 experiment: mpv-direct = wiliwili-style network loadfile.
    {
        const int mode = getIntOption(SettingItem::PLAYER_STREAM_MODE);
        MPVCore::ALLOW_NETWORK_URL = (mode == 1);
        if (MPVCore::ALLOW_NETWORK_URL) {
            // Small demuxer cache helps network HLS (wiliwili wiki ~10-20MB).
            MPVCore::INMEMORY_CACHE = 20;
#if defined(__SWITCH__)
            aniswitchStartupLog("PERF: stream mode=mpv-direct allow_net=1 cache=20MB");
#endif
        } else {
            MPVCore::INMEMORY_CACHE = 0;
#if defined(__SWITCH__)
            aniswitchStartupLog("PERF: stream mode=seamless allow_net=0");
#endif
        }
    }
#if defined(__SWITCH__)
    MPVCore::HARDWARE_DEC = true;
    MPVCore::PLAYER_HWDEC_METHOD = "auto";
    // No in-memory demuxer cache: SD stream + 256MB heap + deko3d FBO
    // already pressure the app budget.  Cache stays off; download-then-
    // local-mpv keeps the file on sdmc.
    MPVCore::INMEMORY_CACHE = 0;
    aniswitchStartupLog("PERF: mpv statics hwdec=on method=auto memCache=0");
#else
    {
        // playerHwdec option list is {"auto","off","on"} raw {0,1,2}.
        // getBoolOption cannot express "auto" vs "on"; treat raw>0 as on
        // except when the stored choice is explicitly "off" (raw==1) and
        // default is "off" — desktop keeps prior behaviour (off unless on).
        const int raw = getIntOption(SettingItem::PLAYER_HWDEC);
        MPVCore::HARDWARE_DEC = (raw == 2);  // only explicit "on"
        if (raw == 0) {
            // "auto": enable on desktop too (mpv picks a safe method).
            MPVCore::HARDWARE_DEC = true;
            MPVCore::PLAYER_HWDEC_METHOD = "auto-safe";
        }
    }
#endif
}

void ProgramConfig::load() {
    std::string path = getConfigDir() + "/ani-switch.json";
    std::ifstream f(path);
    if (!f.is_open()) {
        brls::Logger::info("ProgramConfig: no config at {}, using defaults", path);
        return;
    }
    try {
        nlohmann::json j;
        f >> j;
        if (j.contains("setting") && j["setting"].is_object()) setting = j["setting"];
        if (j.contains("searchHistory") && j["searchHistory"].is_array()) {
            searchHistory = j["searchHistory"].get<std::vector<std::string>>();
        }
        if (j.contains("bangumiAccessToken") && j["bangumiAccessToken"].is_string()) {
            bangumiAccessToken = j["bangumiAccessToken"];
        }
        if (j.contains("bangumiRefreshToken") && j["bangumiRefreshToken"].is_string()) {
            bangumiRefreshToken = j["bangumiRefreshToken"];
        }
        if (j.contains("bangumiTokenExpiry") && j["bangumiTokenExpiry"].is_number_integer()) {
            bangumiTokenExpiry = j["bangumiTokenExpiry"];
        }
        if (j.contains("bangumiUserId") && j["bangumiUserId"].is_string()) {
            bangumiUserId = j["bangumiUserId"];
        }
        if (j.contains("bangumiClientId") && j["bangumiClientId"].is_string()) {
            bangumiClientId = j["bangumiClientId"];
        }
        if (j.contains("bangumiClientSecret") && j["bangumiClientSecret"].is_string()) {
            bangumiClientSecret = j["bangumiClientSecret"];
        }
        if (j.contains("device") && j["device"].is_string()) device = j["device"];
        if (j.contains("client") && j["client"].is_string()) client = j["client"];
        // v20.0: persist HTTP proxy (Clash/V2Ray on LAN) and ani JWT.
        // These were public members with getters/setters but never
        // written to / read from ani-switch.json — a settings toggle
        // could not survive a restart.
        if (j.contains("httpProxy") && j["httpProxy"].is_string()) {
            httpProxy = j["httpProxy"].get<std::string>();
        }
        if (j.contains("httpsProxy") && j["httpsProxy"].is_string()) {
            httpsProxy = j["httpsProxy"].get<std::string>();
        }
        if (j.contains("aniAccessToken") && j["aniAccessToken"].is_string()) {
            aniAccessToken = j["aniAccessToken"].get<std::string>();
        }
        if (j.contains("aniRefreshToken") && j["aniRefreshToken"].is_string()) {
            aniRefreshToken = j["aniRefreshToken"].get<std::string>();
        }
        if (j.contains("aniExpiresAtMillis") && j["aniExpiresAtMillis"].is_number_integer()) {
            aniExpiresAtMillis = j["aniExpiresAtMillis"].get<int64_t>();
        }
        if (j.contains("aniUserId") && j["aniUserId"].is_string()) {
            aniUserId = j["aniUserId"].get<std::string>();
        }
        if (j.contains("aniBangumiPat") && j["aniBangumiPat"].is_string()) {
            aniBangumiPat = j["aniBangumiPat"].get<std::string>();
        }
        // v17.3: first-run gate for onboarding.  Default
        // false when the key is missing so a fresh install
        // goes straight to MainActivity; the onboarding can
        // be triggered manually from Settings → "重新走引导".
        // v17.6 changed the default from true to false after
        // the OnboardingActivity turned out to crash on
        // first-frame layout in some configurations.
        if (j.contains("firstRun") && j["firstRun"].is_boolean()) {
            firstRun_ = j["firstRun"].get<bool>();
        } else {
            firstRun_ = false;
        }
        // v22.2 first-run 输入码 prompt state.
        firstRunCodePromptShown_ =
            j.value("firstRunCodePromptShown", false);
        firstRunCodeSkipped_ =
            j.value("firstRunCodeSkipped", false);
        pendingFirstRunCode_ =
            j.value("pendingFirstRunCode", std::string());
    } catch (const std::exception& e) {
        brls::Logger::error("ProgramConfig::load: damaged config: {}", e.what());
    }
    brls::Logger::info("ProgramConfig::load: loaded from {}", path);
}

bool ProgramConfig::isFirstRun() const {
    return firstRun_;
}

void ProgramConfig::markFirstRunDone() {
    firstRun_ = false;
    save();
}

void ProgramConfig::resetFirstRun() {
    firstRun_ = true;
    // Re-run onboarding should offer the login-code prompt again.
    firstRunCodePromptShown_ = false;
    firstRunCodeSkipped_     = false;
    pendingFirstRunCode_.clear();
    save();
}

bool ProgramConfig::isFirstRunCodePromptShown() const {
    return firstRunCodePromptShown_;
}
void ProgramConfig::setFirstRunCodePromptShown(bool shown) {
    firstRunCodePromptShown_ = shown;
    save();
}
bool ProgramConfig::isFirstRunCodeSkipped() const {
    return firstRunCodeSkipped_;
}
void ProgramConfig::setFirstRunCodeSkipped(bool skipped) {
    firstRunCodeSkipped_ = skipped;
    save();
}
void ProgramConfig::setPendingFirstRunCode(const std::string& code) {
    pendingFirstRunCode_ = code;
    save();
}
std::string ProgramConfig::getPendingFirstRunCode() const {
    return pendingFirstRunCode_;
}

void ProgramConfig::save() {
    std::string dir = getConfigDir();
    std::error_code error;
    std::filesystem::create_directories(dir, error);
    std::string path = dir + "/ani-switch.json";
    std::ofstream f(path);
    if (!f.is_open()) {
        brls::Logger::error("ProgramConfig::save: cannot open {}", path);
        return;
    }
    nlohmann::json j;
    j["setting"]             = setting;
    j["searchHistory"]       = searchHistory;
    j["bangumiAccessToken"]  = bangumiAccessToken;
    j["bangumiRefreshToken"] = bangumiRefreshToken;
    j["bangumiTokenExpiry"]  = bangumiTokenExpiry;
    j["bangumiUserId"]       = bangumiUserId;
    j["bangumiClientId"]     = bangumiClientId;
    j["bangumiClientSecret"] = bangumiClientSecret;
    j["device"]              = device;
    j["client"]              = client;
    j["httpProxy"]           = httpProxy;
    j["httpsProxy"]          = httpsProxy;
    j["aniAccessToken"]      = aniAccessToken;
    j["aniRefreshToken"]     = aniRefreshToken;
    j["aniExpiresAtMillis"]  = aniExpiresAtMillis;
    j["aniUserId"]           = aniUserId;
    j["aniBangumiPat"]       = aniBangumiPat;
    j["firstRun"]            = firstRun_;   // v17.3 onboarding gate
    j["firstRunCodePromptShown"] = firstRunCodePromptShown_;  // v22.2
    j["firstRunCodeSkipped"]     = firstRunCodeSkipped_;
    j["pendingFirstRunCode"]     = pendingFirstRunCode_;
    f << j.dump(2);
}

void ProgramConfig::exit(char* /*argv*/[]) {
    save();
}

void ProgramConfig::setBangumiToken(const std::string& access,
                                    const std::string& refresh,
                                    int64_t expiresIn,
                                    const std::string& userId) {
    bangumiAccessToken  = access;
    bangumiRefreshToken = refresh;
    bangumiTokenExpiry = expiresIn > 0 ? static_cast<int64_t>(time(nullptr)) + expiresIn : 0;
    bangumiUserId       = userId;
    save();
}

std::string ProgramConfig::getBangumiAccessToken() const { return bangumiAccessToken; }
std::string ProgramConfig::getBangumiRefreshToken() const { return bangumiRefreshToken; }
int64_t     ProgramConfig::getBangumiTokenExpiry()  const { return bangumiTokenExpiry; }
std::string ProgramConfig::getUserID() const { return bangumiUserId; }

bool ProgramConfig::hasLoginInfo() const {
    return !bangumiAccessToken.empty() && (bangumiTokenExpiry == 0 || bangumiTokenExpiry > time(nullptr));
}

void ProgramConfig::logout() {
    bangumiAccessToken.clear();
    bangumiRefreshToken.clear();
    bangumiTokenExpiry = 0;
    bangumiUserId.clear();
    save();
}

void ProgramConfig::setBangumiClientCredentials(const std::string& clientId,
                                                const std::string& clientSecret) {
    bangumiClientId     = clientId;
    bangumiClientSecret = clientSecret;
    save();
}
std::string ProgramConfig::getBangumiClientId() const     { return bangumiClientId; }
std::string ProgramConfig::getBangumiClientSecret() const { return bangumiClientSecret; }
bool        ProgramConfig::hasBangumiClientCredentials() const {
    return !bangumiClientId.empty() && !bangumiClientSecret.empty();
}

namespace {
std::string g_pkceVerifier;   // PKCE in-flight verifier, process-lifetime
}
void ProgramConfig::setBangumiPKCEVerifier(const std::string& verifier) { g_pkceVerifier = verifier; }
std::string ProgramConfig::getBangumiPKCEVerifier() const { return g_pkceVerifier; }
void ProgramConfig::clearBangumiPKCEVerifier() { g_pkceVerifier.clear(); }

void ProgramConfig::addSearchHistory(const std::string& key) {
    if (key.empty()) return;
    auto it = std::find(searchHistory.begin(), searchHistory.end(), key);
    if (it != searchHistory.end()) searchHistory.erase(it);
    searchHistory.insert(searchHistory.begin(), key);
    if (searchHistory.size() > 30) searchHistory.resize(30);
    save();
}

std::vector<std::string> ProgramConfig::getSearchHistory() const {
    return searchHistory;
}

void ProgramConfig::clearSearchHistory() {
    searchHistory.clear();
    save();
}

ProgramOption ProgramConfig::getOptionData(SettingItem item) {
    return SETTING_MAP.at(item);
}

int ProgramConfig::getIntOption(SettingItem item) {
    const auto& opt = SETTING_MAP.at(item);
    int idx = getSettingItem<int>(item, static_cast<int>(opt.defaultOption));
    if (idx < 0 || idx >= (int)opt.rawOptionList.size()) {
        idx = static_cast<int>(opt.defaultOption);
    }
    return opt.rawOptionList[idx];
}

bool ProgramConfig::getBoolOption(SettingItem item) {
    return getIntOption(item) != 0;
}

void ProgramConfig::setProxy(const std::string& proxy) {
    httpProxy = proxy;
    save();
    HTTP::applyProxy(proxy);
}
std::string ProgramConfig::getProxy() const { return httpProxy; }

void Register::initCustomView() {
    // v22 TsVitch player graft — XML views used by player_tsvitch.xml
    brls::Application::registerXMLView("VideoView", ::VideoView::create);
    brls::Application::registerXMLView("SVGImage", ::SVGImage::create);
    brls::Application::registerXMLView("VideoProfile", ::VideoProfile::create);
    brls::Application::registerXMLView("VideoProgressSlider", ::VideoProgressSlider::create);
    brls::Application::registerXMLView("HintLabel", ::HintLabel::create);
}
void Register::initCustomTheme() { /* themes applied via XML layouts */ }
void Register::initCustomStyle() { /* styles applied via XML layouts */ }
const std::string& Register::getCustomThemeColorHex() { return customThemeColorHex; }
bool Register::isBangumiPink(const std::string& color) {
    return color == "#F09199" || color == "#F09199FF";
}

}  // namespace aniswitch
