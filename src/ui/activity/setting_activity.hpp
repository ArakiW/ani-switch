// SPDX-License-Identifier: AGPL-3.0
//
// Settings. A flat list of toggles; each toggle writes back into
// ProgramConfig. v0.1 only has the bare minimum.

#pragma once

#include <borealis.hpp>

namespace aniswitch {

class SettingActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/setting_activity.xml");
    SettingActivity();
    void onContentAvailable() override;
};

}  // namespace aniswitch
