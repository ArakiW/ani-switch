// SPDX-License-Identifier: AGPL-3.0
//
// v18.8: log activity.  Reads sdmc:/switch/aniswitch/startup.log
// and shows the contents in a ScrollingFrame.  Two buttons:
// "重新读取" (re-read the file) and "清空日志" (truncate,
// after a confirm dialog).
//
// We hand the file path to the user verbatim — animeko's
// LogTab (ui-settings/tabs/log/LogTab.kt) is a live tail
// against an in-memory ring buffer, but our aniswitch logs
// go straight to startup.log from switch_wrapper.c (the
// libnx userAppInit bridge), so reading the file is the
// closest we get to a "live tail" without rewriting the
// log layer.

#pragma once

#include <borealis.hpp>

namespace aniswitch {

class LogActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/log_activity.xml");
    LogActivity();
    void onContentAvailable() override;
private:
    void reload();
    brls::Label* statusLabel_  = nullptr;
    brls::Box*   logContainer_ = nullptr;
};

}  // namespace aniswitch
