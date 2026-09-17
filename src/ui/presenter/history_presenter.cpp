// SPDX-License-Identifier: AGPL-3.0

#include "ui/presenter/history_presenter.hpp"
#include "core/history.hpp"
#include "utils/thread_helper.hpp"

namespace aniswitch {

void HistoryPresenter::refresh(int limit) {
    auto callback = uiCallback([this](std::vector<SQLiteStore::HistoryEntry> entries) {
        onHistory.fire(std::move(entries));
    });
    submit_detached([callback, limit]() {
        callback(HistoryManager::instance().recent(limit));
    });
}

}  // namespace aniswitch
