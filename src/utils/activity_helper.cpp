// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)

#include "utils/activity_helper.hpp"
#include <borealis.hpp>
#include <borealis/core/logger.hpp>

#if defined(__SWITCH__) && defined(ANISWITCH_SWITCH_DEBUG)
extern "C" void aniswitchStartupLog(const char* message);
#endif

namespace aniswitch {

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
