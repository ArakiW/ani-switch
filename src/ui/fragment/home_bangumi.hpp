// SPDX-License-Identifier: AGPL-3.0
//
// "Daily broadcast" / weekly schedule fragment. Lists one
// SubjectCell per CalendarItem, grouped by weekday (today is
// highlighted).
#pragma once
#include <borealis.hpp>
#include "ui/presenter/home_presenter.hpp"

namespace aniswitch {
class StatusView;
class HomeBangumiFragment : public brls::Box {
public:
    HomeBangumiFragment();
    ~HomeBangumiFragment() override;
    void refresh();
private:
    void onCalendar(std::vector<CalendarItem> v);
    void armTimeout();
    brls::Box* list_ = nullptr;
    StatusView* status_ = nullptr;
    HomePresenter presenter_;
    // v17.6.2: timeout state.  loading_ tells the timer not
    // to overwrite a successful state; timeoutToken_ identifies
    // the in-flight brls::delay so we can cancel it when a
    // refresh() comes in early.
    bool    loading_ = true;
    size_t  timeoutToken_ = 0;
};
}
