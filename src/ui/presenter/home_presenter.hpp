// SPDX-License-Identifier: AGPL-3.0
//
// Home tab presenter. Pulls:
//
//  1. Bangumi /v0/calendar        — daily broadcast (replaces Bangumi
//                                  "今日放送" tab from the official web)
//  2. Bangumi /v0/search/subjects — trending / popular search keywords
//                                  (we treat the response as a "what's
//                                  hot right now" recommendation list)
//
// The two lists are exposed as events; the home activity subscribes
// and renders them in two scrolling rows.

#pragma once

#include "ui/presenter/presenter.hpp"
#include "net/bgm_types.hpp"
#include <borealis/core/event.hpp>
#include <vector>

namespace aniswitch {

class HomePresenter : public Presenter {
public:
    brls::Event<std::vector<CalendarItem>>    onCalendar;
    brls::Event<std::vector<SearchSubject>>  onTrending;

    void refresh();
    void fetchTrending(const std::string& keyword = "热门");
};

}  // namespace aniswitch
