// SPDX-License-Identifier: AGPL-3.0
//
// Episode list presenter. Surfaces a "continue at last watched" entry
// plus the full episode list, fetched from Bangumi + local SQLite.

#pragma once

#include "ui/presenter/presenter.hpp"
#include "net/bgm_types.hpp"
#include "utils/sqlite_store.hpp"
#include <borealis/core/event.hpp>
#include <optional>

namespace aniswitch {

class EpisodePresenter : public Presenter {
public:
    brls::Event<std::vector<Episode>>                  onEpisodes;
    brls::Event<std::optional<SQLiteStore::HistoryEntry>> onLastWatched;

    void setSubjectId(int32_t id) { subjectId_ = id; }
    void refresh();

private:
    int32_t subjectId_ = 0;
};

}  // namespace aniswitch
