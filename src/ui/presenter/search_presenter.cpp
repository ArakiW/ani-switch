// SPDX-License-Identifier: AGPL-3.0

#include "ui/presenter/search_presenter.hpp"
#include "net/ani_client.hpp"
#include "net/bgm_client.hpp"
#include "utils/config_helper.hpp"
#include <borealis/core/logger.hpp>

namespace aniswitch {

void SearchPresenter::search(const std::string& keyword) {
    uint64_t ticket = ++inflight_;
    if (keyword.empty()) {
        onResults.fire({});
        return;
    }
    // v21: Ani search first (same data plane as home), Bangumi fallback.
    AniClient::searchSubjects(keyword, 25,
        uiCallback([this, ticket, keyword](std::vector<SearchSubject> list) {
            if (ticket != inflight_.load()) return;
            if (!list.empty()) {
                onResults.fire(std::move(list));
                return;
            }
            BangumiClient::searchSubjects(keyword, 25, 0, "match",
                uiCallback([this, ticket](SearchSubjectList l) {
                    if (ticket != inflight_.load()) return;
                    onResults.fire(std::move(l.data));
                }),
                [this, ticket](const std::string& m, int) {
                    brls::Logger::warning("search bangumi fallback: {}", m);
                    if (ticket == inflight_.load()) onResults.fire({});
                });
        }),
        [this, ticket, keyword](const std::string& m, int) {
            brls::Logger::warning("search ani: {}", m);
            BangumiClient::searchSubjects(keyword, 25, 0, "match",
                uiCallback([this, ticket](SearchSubjectList l) {
                    if (ticket != inflight_.load()) return;
                    onResults.fire(std::move(l.data));
                }),
                [this, ticket](const std::string& m2, int) {
                    brls::Logger::warning("search bangumi: {}", m2);
                    if (ticket == inflight_.load()) onResults.fire({});
                });
        });
}

void SearchPresenter::clearHistory() {
    ProgramConfig::instance().clearSearchHistory();
    onHistory.fire({});
}

void SearchPresenter::loadHistory() {
    onHistory.fire(ProgramConfig::instance().getSearchHistory());
}

void SearchPresenter::remember(const std::string& keyword) {
    ProgramConfig::instance().addSearchHistory(keyword);
    loadHistory();
}

}  // namespace aniswitch
