// SPDX-License-Identifier: AGPL-3.0
//
// Reusable loading / error / empty placeholder view.  Drop this
// into a screen when data is fetching (Loading), has failed
// (Error, optional retry), or has no results (Empty).  Centralising
// these three states keeps the screens consistent and means the
// user can scan a screen and instantly know whether to wait, retry,
// or pick a different item.
//
// Usage:
//   auto* status = new StatusView();
//   status->setLoading("加载每日放送...");
//   // later:
//   status->setEmpty("本周没有新番剧");
//   // or:
//   status->setError("网络错误", []{ presenter_.refresh(); });

#pragma once
#include <borealis.hpp>
#include <functional>
#include <string>

namespace aniswitch {

class StatusView : public brls::Box {
public:
    enum class State { Loading, Empty, Error };

    StatusView();

    void setLoading(const std::string& message = "");
    void setEmpty(const std::string& message = "");
    void setError(const std::string& message,
                  std::function<void()> onRetry = nullptr);

    State state() const { return state_; }

private:
    State state_ = State::Loading;
    brls::Box*        iconBox_   = nullptr;
    brls::Label*      title_     = nullptr;
    brls::Label*      detail_    = nullptr;
    brls::Button*     retry_     = nullptr;
    brls::ProgressSpinner* spinner_ = nullptr;
    std::function<void()> onRetry_;
};

}  // namespace aniswitch
