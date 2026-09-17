// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
//
// Intent system: a thin wrapper around brls::Application::pushActivity
// that lets us route to specific screens with semantic names instead of
// stack-manipulating brls::Activity* everywhere.

#pragma once

#include <string>
#include <cstdint>

namespace aniswitch {

class Intent {
public:
    // Main entry: home / recommend / bangumi daily / rank tabs.
    static void openMain();

    // Generic "first time user" tutorial.
    static void openHint();

    // v17.3: 5-step onboarding shown on first run.
    static void openOnboarding();

    // Settings screen.
    static void openSetting();

    // ---- Subject (bangumi) navigation -------------------------------------
    // Open a subject by its Bangumi ID. type is "anime" / "real" / "book" /
    // "music" / "game" / "all".
    static void openSubject(int32_t subjectId, const std::string& type = "anime");

    // Open the episode list for a subject, optionally resuming to a specific
    // episode id (e.g. from history).
    static void openEpisodeList(int32_t subjectId, int32_t resumeEpisodeId = 0);

    // Open a Bangumi person (voice actor / staff) detail page by
    // person id.  Currently used by the subject detail cast chips.
    static void openPerson(int32_t personId);

    // ---- Player ----------------------------------------------------------
    // Play a specific episode.
    //   - danmakuSource: "dandanplay" or "myani"
    //   - videoSource:   "http" (only one supported in v0.1)
    static void openPlayer(int32_t episodeId,
                          const std::string& danmakuSource = "dandanplay",
                          const std::string& videoSource   = "http",
                          int64_t resumePositionMs = 0);

    // ---- User content ----------------------------------------------------
    static void openMyCollection();
    static void openHistory();
    static void openSearch(const std::string& query = "");
    static void openLogin();

    // v17.4: local SD-card video browser.  Used by the
    // settings → "本地视频" entry and any future place that
    // wants to point at a known file on the SD card.
    static void openLocalVideo();

    // v18.4: Bangumi OAuth sync page.  Shows the current
    // access/refresh token state and lets the user force-refresh
    // or sign out from the local app.
    static void openBangumiSync();

    // v18.5: ani server email-OTP login.  Two-step:
    // openEmailLoginStart (enter email + send OTP) and
    // openEmailLoginVerify (enter 6-digit code + finish).
    static void openEmailLoginStart();
    static void openEmailLoginVerify(const std::string& otpId,
                                    const std::string& email,
                                    bool hasExistingUser);

    // v18.6: tabbed SettingsScreen (animeko SettingsScreen
    // ported to borealis).  Replaces the flat SettingActivity
    // entry from v17.0.
    static void openSettings();

    // v18.7: ThemePreviewActivity (animeko ThemePreviewPanel +
    // DarkModeSelectPanel ported as one activity).
    static void openThemePreview();

    // v18.8: LogActivity (animeko LogTab + DebugTab — startup.log
    // viewer with re-read + clear buttons).
    static void openLog();
};

}  // namespace aniswitch
