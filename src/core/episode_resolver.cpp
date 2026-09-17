// SPDX-License-Identifier: AGPL-3.0
#include "core/episode_resolver.hpp"
#include "core/web_selector_provider.hpp"
#include "net/bgm_client.hpp"
#include "utils/sqlite_store.hpp"
#include <borealis/core/logger.hpp>
#if defined(__SWITCH__)
extern "C" void aniswitchStartupLog(const char* message);
#endif

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
#if defined(__SWITCH__)
    {
        char b[100];
        snprintf(b, sizeof(b), "EpisodeResolver: begin ep=%d subj=%d",
                 episodeId, subjectId);
        aniswitchStartupLog(b);
    }
#endif
    // v22: sources.json HTTP sources are keyed by episodeId and need
    // no Bangumi metadata. Try them first so online play works even
    // when api.bgm.tv is slow/blocked.
    SourceManager::instance().enumerate(episodeId,
        [subjectId, episodeId, callback, error](std::vector<VideoSource> sources) {
            if (!sources.empty()) {
                ResolvedEpisode result;
                result.episodeId = episodeId;
                result.bangumiSubjectId = subjectId;
                result.title = "ep " + std::to_string(episodeId);
                result.episodeNumber = 0;
                result.sources = std::move(sources);
                brls::Logger::info("EpisodeResolver: {} local/http source(s), skip Bangumi meta",
                                   result.sources.size());
                if (callback) callback(std::move(result));
                return;
            }
            // Fall through: Bangumi metadata → WebSelector scrape.
            HTTP::getResult<Episode>(
                BangumiClient::baseUrl() + "/v0/episodes/" + std::to_string(episodeId), {},
                [subjectId, callback, error](Episode episode) {
                    ResolvedEpisode result;
                    result.episodeId = episode.id;
                    result.bangumiSubjectId =
                        episode.subjectID ? episode.subjectID : subjectId;
                    result.title =
                        episode.nameCN.empty() ? episode.name : episode.nameCN;
                    result.episodeNumber = episode.sort;
                    result.durationMs =
                        static_cast<int64_t>(episode.duration) * 1000;
                    if (auto progress = SQLiteStore::instance().getProgress(episode.id))
                        result.resumePositionMs = progress->positionMs;
                    if (gWebSelector) {
                        gWebSelector->setContext(episode.id, episode.nameCN,
                                                 episode.name, episode.sort,
                                                 result.title);
                    }
                    SourceManager::instance().enumerate(
                        episode.id,
                        [result, callback](std::vector<VideoSource> sources) mutable {
                            result.sources = std::move(sources);
                            if (callback) callback(std::move(result));
                        },
                        error);
                },
                error);
        },
        error);
}
}  // namespace aniswitch
