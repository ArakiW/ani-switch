// SPDX-License-Identifier: AGPL-3.0
//
// v18.6: animeko 6.1.0 `ui-settings/SettingsScreen` ported as a
// top-level SettingsActivity with a TabFrame.  Five tabs:
//   1. 通用 (AppSettingsTab) — theme / hardware decode /
//      danmaku toggles (lifted from the v17.0 flat setting
//      page).
//   2. Bangumi 同步 (AccountTab) — Bangumi OAuth login +
//      BangumiSyncTab + ani EmailLogin.
//   3. 关于 (AboutTab) — version + license + acknowledgements.
//   4. 调试 (DebugTab) — startup.log path + last lines.
//
// animeko's full settings tree has 11+ tabs (network, media
// selector, theme preview, log, debug, about, account,
// update, etc.).  v18.6 ships the four most-used and leaves
// the rest for v18.7+.

#pragma once

#include <borealis.hpp>

namespace aniswitch {

class SettingsActivity : public brls::Activity {
public:
    // TabFrame multi-page path is disabled (device crash); flat
    // SettingActivity is used instead. Keep a content view so a
    // future re-enable cannot null-deref in setContentView.
    CONTENT_FROM_XML_RES("activity/settings_activity.xml");
    SettingsActivity();
    void onContentAvailable() override;
};

}  // namespace aniswitch
