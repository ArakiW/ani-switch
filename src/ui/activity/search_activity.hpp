// SPDX-License-Identifier: AGPL-3.0
//
// Search activity. Bangumi /v0/search/subjects + a small text input.

#pragma once

#include <borealis.hpp>
#include "ui/presenter/search_presenter.hpp"

namespace aniswitch {

class SearchActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/search_activity.xml");
    SearchActivity(const std::string& initialQuery = "");
    void onContentAvailable() override;
    void onResume() override;

private:
    std::string initial_;
    SearchPresenter presenter_;
    brls::Box* root_ = nullptr;
    brls::EditTextDialog* input_ = nullptr;
    brls::Box* resultsList_ = nullptr;
    brls::View* lastFocused_ = nullptr;

    void onHistory(std::vector<std::string> hist);
    void onResults(std::vector<SearchSubject> res);
    void showInput();
    void doSearch(const std::string& q);
};

}  // namespace aniswitch
