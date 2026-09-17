// SPDX-License-Identifier: AGPL-3.0
//
// Base presenter (MVVM "P"). Each activity owns one or more
// presenters; presenters own the API calls and the in-memory state.
// Activities / fragments subscribe to presenter state changes via
// brls::Event<>'s and update their views accordingly.

#pragma once

#include <borealis/core/event.hpp>
#include <borealis/core/thread.hpp>
#include <memory>
#include <tuple>
#include <utility>
#include <string>

namespace aniswitch {

class Presenter {
public:
    virtual ~Presenter() = default;

    virtual void onViewCreated() {}
    virtual void onViewWillAppear() {}
    virtual void onViewWillDisappear() {}

protected:
    template <typename F>
    auto uiCallback(F callback) {
        std::weak_ptr<int> lifetime = lifetime_;
        return [lifetime, callback](auto... args) {
            brls::sync([lifetime, callback, values = std::make_tuple(std::move(args)...)]() mutable {
                if (!lifetime.expired()) std::apply(callback, std::move(values));
            });
        };
    }

private:
    std::shared_ptr<int> lifetime_ = std::make_shared<int>(0);
};

}  // namespace aniswitch
