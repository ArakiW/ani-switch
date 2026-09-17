// SPDX-License-Identifier: AGPL-3.0
//
// First-time hint activity. Shown on first run from applet mode.

#pragma once

#include <borealis.hpp>

namespace aniswitch {

class HintActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/hint_activity.xml");
    HintActivity();
    void onContentAvailable() override;
};

}  // namespace aniswitch
