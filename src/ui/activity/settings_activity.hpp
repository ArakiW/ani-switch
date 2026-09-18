// SPDX-License-Identifier: AGPL-3.0
//
// v18.6/v22: multi-page Settings twin. NOT the live settings path —
// Intent::openSettings() routes to SettingActivity (make_setting_activity)
// since v20.11 (this class native-crashed after TabFrame). Kept compiled
// and chrome-aligned for a future re-enable; flat list only, no TabFrame.
//
// Groups: 通用 / 播放 / 网络 / 账号 / 关于 / 调试.

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
