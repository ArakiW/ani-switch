// SPDX-License-Identifier: AGPL-3.0
#pragma once
#include <borealis.hpp>
#include <string>
#include <vector>
#include "ui/presenter/home_presenter.hpp"

namespace aniswitch {
class StatusView;
class HomeRankFragment : public brls::Box {
public:
    HomeRankFragment();
    ~HomeRankFragment() override;
    void refresh();
private:
    struct CoverJob {
        brls::Image* view = nullptr;
        std::string url;
        int32_t id = 0;
    };
    void onTrending(std::vector<SearchSubject> v);
    void armTimeout();
    void startCoverLoads();
    brls::Box* list_ = nullptr;      // horizontal rail content (ROW)
    brls::Box* listRows_ = nullptr;  // vertical fallback list
    StatusView* status_ = nullptr;
    HomePresenter presenter_;
    bool    loading_ = true;
    size_t  timeoutToken_ = 0;
    size_t  coverToken_ = 0;
    std::vector<CoverJob> covers_;
};
}
