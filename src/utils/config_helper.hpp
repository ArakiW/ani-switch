// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
//
// Persistent user settings. We use a single nlohmann::json blob in
// <config_dir>/ani-switch.json, identical pattern to wiliwili.
//
// Bangumi-specific keys (OAuth token, user ID) replace the B站 cookie
// fields. There is no WBI signing anymore.

#pragma once

#include <map>
#include <string>
#include <unordered_set>
#include <vector>
#include <nlohmann/json.hpp>
#include <borealis/core/singleton.hpp>

namespace aniswitch {

enum class SettingItem {
    HIDE_BOTTOM_BAR,
    APP_THEME,
    APP_LANG,
    APP_SWAP_ABXY,
    SCROLL_SPEED,
    PLAYER_AUTO_PLAY,
    PLAYER_LOW_QUALITY,
    PLAYER_INMEMORY_CACHE,
    PLAYER_HWDEC,
    // 0 = seamless local ani:// stream (default); 1 = mpv direct network
    // (wiliwili-style loadfile http/https/m3u8 + optional http-proxy).
    PLAYER_STREAM_MODE,
    PLAYER_VOLUME,
    PLAYER_SPEED,
    DANMAKU_ON,
    DANMAKU_FILTER_LEVEL,
    DANMAKU_FILTER_SHOW_TOP,
    DANMAKU_FILTER_SHOW_BOTTOM,
    DANMAKU_FILTER_SHOW_SCROLL,
    DANMAKU_FILTER_SHOW_COLOR,
    DANMAKU_FILTER_SHOW_ADVANCED,
    DANMAKU_SMART_MASK,
    DANMAKU_STYLE_AREA,
    DANMAKU_STYLE_ALPHA,
    DANMAKU_STYLE_FONTSIZE,
    DANMAKU_STYLE_LINE_HEIGHT,
    DANMAKU_STYLE_SPEED,
    DANMAKU_STYLE_FONT,
    DANMAKU_RENDER_QUALITY,
    TEXTURE_CACHE_NUM,
    OPENCC_ON,
    IMAGE_REQUEST_THREADS,
    HTTP_PROXY,
    HTTP_PROXY_STATUS,
    TLS_VERIFY,
    HTTP_TIMEOUT,
    HTTP_CONNECTION_TIMEOUT,
    HTTP_DNS_CACHE_TIMEOUT,
    KEYMAP,
    LIMITED_FPS,
    SWAP_INTERVAL,
    HOME_WINDOW_STATE,
    SHORTCUT_REFRESH,
    SHORTCUT_SEARCH,
    SHORTCUT_LAST,
    SHORTCUT_NEXT,
    SHORTCUT_VOLUME_UP,
    SHORTCUT_VOLUME_DOWN,
    SHORTCUT_SETTING,
    SHORTCUT_DANMAKU,
    SHORTCUT_FORWARD,
    SHORTCUT_REWIND,
    SHORTCUT_VIDEO_PAUSE,
    CUSTOM_THEME_COLOR,
};

typedef std::map<std::string, std::string> Cookie;

class CustomTheme {
public:
    std::string id;
    std::string name;
    std::string desc;
    std::string version;
    std::string author;
    std::string path;
};

struct ProgramOption {
    std::string key;
    std::vector<std::string> optionList;
    std::vector<int> rawOptionList;
    size_t defaultOption = 0;
};

class ProgramConfig : public brls::Singleton<ProgramConfig> {
public:
    ProgramConfig();

    // ---- Bangumi auth ----------------------------------------------------
    void setBangumiToken(const std::string& access, const std::string& refresh,
                        int64_t expiresIn, const std::string& userId);

    // ---- First-run onboarding (v17.3) ------------------------------------
    // We use a top-level bool "first_run" in ani-switch.json so the
    // onboarding activity can decide whether to gate MainActivity.
    // A fresh install has no key, which is treated as first run.
    bool isFirstRun() const;
    void markFirstRunDone();
    void resetFirstRun();  // for "重新走引导" settings entry
    // v22.2 first-run 输入码 prompt.
    // shown  — dialog has been presented at least once (don't re-nag).
    // skipped — user pressed 跳过; remembered across launches.
    // pending — short (<24 char) code attempt stored for later PKCE paste.
    bool isFirstRunCodePromptShown() const;
    void setFirstRunCodePromptShown(bool shown);
    bool isFirstRunCodeSkipped() const;
    void setFirstRunCodeSkipped(bool skipped);
    void setPendingFirstRunCode(const std::string& code);
    std::string getPendingFirstRunCode() const;
    std::string getBangumiAccessToken() const;
    std::string getBangumiRefreshToken() const;
    int64_t    getBangumiTokenExpiry() const;
    std::string getUserID() const;
    bool hasLoginInfo() const;
    void logout();

    // OAuth app credentials (registered at https://bgm.tv/dev/app).
    // Stored as plain strings in ani-switch.json; the file is
    // user-owned so this is no worse than any other config secret.
    void setBangumiClientCredentials(const std::string& clientId,
                                     const std::string& clientSecret);
    std::string getBangumiClientId() const;
    std::string getBangumiClientSecret() const;
    bool hasBangumiClientCredentials() const;

    // PKCE code_verifier that pairs with the in-flight authorize URL.
    // In-memory only; cleared on a successful exchange or on logout.
    void        setBangumiPKCEVerifier(const std::string& verifier);
    std::string getBangumiPKCEVerifier() const;
    void        clearBangumiPKCEVerifier();

    // ---- Generic setting storage -----------------------------------------
    template <typename T>
    T getSettingItem(SettingItem item, T defaultValue) {
        auto option = SETTING_MAP.find(item);
        if (option == SETTING_MAP.end()) return defaultValue;
        const auto& key = option->second.key;
        if (!setting.contains(key)) return defaultValue;
        try {
            return this->setting.at(key).get<T>();
        } catch (const std::exception& e) {
            return defaultValue;
        }
    }

    template <typename T>
    void setSettingItem(SettingItem item, T data, bool save = true) {
        setting[SETTING_MAP.at(item).key] = data;
        if (save) this->save();
    }

    ProgramOption getOptionData(SettingItem item);
    int  getIntOption(SettingItem item);
    bool getBoolOption(SettingItem item);

    // ---- Misc ------------------------------------------------------------
    void addSearchHistory(const std::string& key);
    std::vector<std::string> getSearchHistory() const;
    void clearSearchHistory();

    void init();
    void load();
    void save();
    void exit(char* argv[]);

    std::string getConfigDir() const;
    std::string getHomePath() const;

    void setProxy(const std::string& proxy);
    std::string getProxy() const;

    // ---- Public state ----------------------------------------------------
    nlohmann::json setting;
    std::vector<std::string> searchHistory;
    std::string bangumiAccessToken;
    std::string bangumiRefreshToken;
    int64_t    bangumiTokenExpiry = 0;
    std::string bangumiUserId;
    std::string bangumiClientId     = "bgm70826a9f12bfbfc86";
    std::string bangumiClientSecret = "55804d6915d7da7ebd9eb61d9758358c";
    // v18.5: ani server (api.animeko.org) email-OTP login.
    // Sits alongside the Bangumi tokens; the ani email login
    // hands us a Bangumi PAT (bangumiAccessToken from the
    // response), so the user gets a single login that fills
    // both fields.
    std::string aniAccessToken;
    std::string aniRefreshToken;
    int64_t     aniExpiresAtMillis = 0;
    std::string aniUserId;
    std::string aniBangumiPat;  // optional companion PAT
    std::string client;
    std::string device;
    std::string httpProxy;
    std::string httpsProxy;
    // v17.6: default false.  v17.3/v17.4/v17.5 default true
    // was wrong — the onboarding activity crashed on first
    // launch for several users, so we make the gate opt-in via
    // the Settings → "重新走引导" button instead of forcing it
    // on every fresh install.
    bool        firstRun_ = false;  // v17.3 onboarding gate
    // v22.2 first-run login-code prompt state (ani-switch.json).
    bool        firstRunCodePromptShown_ = false;
    bool        firstRunCodeSkipped_     = false;
    std::string pendingFirstRunCode_;

    static std::map<SettingItem, ProgramOption> SETTING_MAP;
};

class Register {
public:
    static void initCustomView();
    static void initCustomTheme();
    static void initCustomStyle();
    static const std::string& getCustomThemeColorHex();
    static bool isBangumiPink(const std::string& color);
private:
    static std::string customThemeColorHex;
};

}  // namespace aniswitch
