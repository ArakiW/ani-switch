// SPDX-License-Identifier: AGPL-3.0
//
// Search presenter. Debounced text search + history list. We use a
// simple in-flight counter so a slow callback from a previous query
// can't overwrite a faster one (Bangumi's search is reasonably fast
// but the latency from the Switch is not negligible).

#pragma once

#include "ui/presenter/presenter.hpp"
#include "net/bgm_types.hpp"
#include <borealis/core/event.hpp>
#include <atomic>
#include <string>
#include <vector>

namespace aniswitch {

class SearchPresenter : public Presenter {
public:
    brls::Event<std::vector<SearchSubject>>  onResults;
    brls::Event<std::vector<std::string>>    onHistory;

    void search(const std::string& keyword);
    void clearHistory();
    void loadHistory();
    void remember(const std::string& keyword);

private:
    std::atomic<uint64_t> inflight_{0};
};

}  // namespace aniswitch
