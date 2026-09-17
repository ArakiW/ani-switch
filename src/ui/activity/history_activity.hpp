// SPDX-License-Identifier: AGPL-3.0
//
// Watch history. Flat list of recent episodes; tap to resume.

#pragma once

#include <borealis.hpp>
#include "ui/presenter/history_presenter.hpp"

namespace aniswitch {

class HistoryActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/history_activity.xml");
    HistoryActivity();
    void onContentAvailable() override;

private:
    HistoryPresenter presenter_;
    brls::Box*   list_    = nullptr;
    brls::Label* summary_ = nullptr;   // v17.5: top summary line
    brls::Button* clearBtn_ = nullptr; // v17.5: "清空所有" button
    void render(const std::vector<SQLiteStore::HistoryEntry>& v);
};

}  // namespace aniswitch
