// SPDX-License-Identifier: AGPL-3.0
#include "ui/presenter/episode_presenter.hpp"
#include "net/bgm_client.hpp"
#include "utils/sqlite_store.hpp"
#include <borealis/core/logger.hpp>

namespace aniswitch {

void EpisodePresenter::refresh() {
    if (subjectId_ == 0) return;
    const int32_t id = subjectId_;
    BangumiClient::getEpisodes(id, 0,
        uiCallback([this, id](std::vector<Episode> e) { if (id == subjectId_) onEpisodes.fire(std::move(e)); }),
        [](const std::string& m, int) { brls::Logger::warning("episodes: {}", m); });
    onLastWatched.fire(SQLiteStore::instance().getLastEpisodeForSubject(id));
}

}  // namespace aniswitch
