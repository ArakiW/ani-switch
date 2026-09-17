// SPDX-License-Identifier: AGPL-3.0
//
// History presenter. Pulls from SQLiteStore.

#pragma once

#include "ui/presenter/presenter.hpp"
#include "utils/sqlite_store.hpp"
#include <borealis/core/event.hpp>
#include <vector>

namespace aniswitch {

class HistoryPresenter : public Presenter {
public:
    brls::Event<std::vector<SQLiteStore::HistoryEntry>> onHistory;

    void refresh(int limit = 100);
};

}  // namespace aniswitch
