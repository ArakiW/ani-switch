// SPDX-License-Identifier: AGPL-3.0
//
// "Recommend" fragment. v20.0: Ani /v1/trends shelf (same data as
// 探索, denser list) so the rail has a third destination that is
// useful offline-empty-state-wise (shows proxyHint on failure).
#pragma once
#include <borealis.hpp>
#include "ui/presenter/home_presenter.hpp"

namespace aniswitch {
class StatusView;
class HomeRecommendFragment : public brls::Box {
public:
    HomeRecommendFragment();
    ~HomeRecommendFragment() override;
    void refresh();
private:
    brls::Box* list_ = nullptr;
    brls::Box* railList_ = nullptr;
    StatusView* status_ = nullptr;
    HomePresenter presenter_;
    bool    loading_ = true;
    size_t  timeoutToken_ = 0;
};
}
