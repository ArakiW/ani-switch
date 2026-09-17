// SPDX-License-Identifier: AGPL-3.0
//
// Factory functions for all activities. activity_helper.cpp uses these
// via the `aniswitch::ui` namespace to construct activities without
// pulling in every activity header (which would create a circular
// dependency through the presenter layer).

#include "ui/activity/main_activity.hpp"
#include "ui/activity/onboarding_activity.hpp"
#include "ui/activity/subject_activity.hpp"
#include "ui/activity/episode_list_activity.hpp"
#include "ui/activity/player_activity.hpp"
#include "ui/activity/source_picker_activity.hpp"
#include "ui/activity/search_activity.hpp"
#include "ui/activity/my_collection_activity.hpp"
#include "ui/activity/history_activity.hpp"
#include "ui/activity/setting_activity.hpp"
#include "ui/activity/settings_activity.hpp"
#include "ui/activity/hint_activity.hpp"
#include "ui/activity/local_video_activity.hpp"
#include "ui/activity/login_activity.hpp"
#include "ui/activity/person_activity.hpp"
#include "ui/activity/bangumi_sync_activity.hpp"
#include "ui/activity/email_login_activity.hpp"
#include "ui/activity/theme_preview_activity.hpp"
#include "ui/activity/log_activity.hpp"
#include <borealis.hpp>

namespace aniswitch::ui {

brls::Activity* make_main_activity()            { return new MainActivity(); }
brls::Activity* make_onboarding_activity()      { return new OnboardingActivity(); }  // v17.3
brls::Activity* make_hint_activity()            { return new HintActivity(); }
brls::Activity* make_setting_activity()         { return new SettingActivity(); }
brls::Activity* make_settings_activity()        { return new SettingsActivity(); }   // v18.6
brls::Activity* make_local_video_activity()     { return new LocalVideoActivity(); } // v17.4
brls::Activity* make_bangumi_sync_activity()    { return new BangumiSyncActivity(); } // v18.4
brls::Activity* make_email_login_activity()     { return new EmailLoginActivity(); } // v18.5
brls::Activity* make_theme_preview_activity()  { return new ThemePreviewActivity(); } // v18.7
brls::Activity* make_log_activity()           { return new LogActivity(); }            // v18.8
brls::Activity* make_subject_activity(int32_t s, const std::string& t) {
    return new SubjectActivity(s, t);
}
brls::Activity* make_episode_list_activity(int32_t s, int32_t r) {
    return new EpisodeListActivity(s, r);
}
brls::Activity* make_player_activity(int32_t e, const std::string& ds,
                                     const std::string& vs, int64_t pos) {
    return new PlayerActivity(e, ds, vs, pos);
}
brls::Activity* make_source_picker_activity(int32_t e, int32_t s,
                                            const std::string& t, bool autoPick) {
    return new SourcePickerActivity(e, s, t, autoPick);
}
brls::Activity* make_my_collection_activity()  { return new MyCollectionActivity(); }
brls::Activity* make_history_activity()        { return new HistoryActivity(); }
brls::Activity* make_search_activity(const std::string& q) { return new SearchActivity(q); }
brls::Activity* make_login_activity()          { return new LoginActivity(); }
brls::Activity* make_person_activity(int32_t p)   { return new PersonActivity(p); }

}  // namespace aniswitch::ui
