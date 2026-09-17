// SPDX-License-Identifier: AGPL-3.0
#include "core/episode_resolver.hpp"
#include "core/web_selector_provider.hpp"
#include "net/bgm_client.hpp"
#include "utils/sqlite_store.hpp"

namespace aniswitch {

namespace {
// The single WebSelectorProvider instance is owned by SourceManager
// registration; we hold a non-owning pointer here.  SourceManager
// outlives any EpisodeResolver callback, so the pointer is stable.
WebSelectorProvider* gWebSelector = nullptr;
}

void EpisodeResolver::setWebSelectorProvider(WebSelectorProvider* p) { gWebSelector = p; }

void EpisodeResolver::resolve(int32_t subjectId, int32_t episodeId, ResolvedCb callback,
                               std::function<void(const std::string&, int)> error) {
    HTTP::getResult<Episode>(BangumiClient::baseUrl() + "/v0/episodes/" + std::to_string(episodeId), {},
        [subjectId, callback, error](Episode episode) {
            ResolvedEpisode result;
            result.episodeId = episode.id;
            result.bangumiSubjectId = episode.subjectID ? episode.subjectID : subjectId;
            result.title = episode.nameCN.empty() ? episode.name : episode.nameCN;
            result.episodeNumber = episode.sort;
            result.durationMs = static_cast<int64_t>(episode.duration) * 1000;
            if (auto progress = SQLiteStore::instance().getProgress(episode.id))
                result.resumePositionMs = progress->positionMs;
            // Hand the WebSelector provider enough context to actually
            // construct search queries.  HTTPSourceProvider does not
            // need this (it keys directly on episodeId).
            if (gWebSelector) {
                gWebSelector->setContext(episode.id, episode.nameCN, episode.name,
                                         episode.sort, result.title);
            }
            SourceManager::instance().enumerate(episode.id,
                [result, callback](std::vector<VideoSource> sources) mutable {
                    result.sources = std::move(sources);
                    if (callback) callback(std::move(result));
                }, error);
        }, error);
}
}
