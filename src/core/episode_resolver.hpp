// SPDX-License-Identifier: AGPL-3.0
//
// "Episode Resolver" is the high-level helper that ties together:
//   1. sources.json (HTTPSourceProvider) keyed by Bangumi episode id,
//      with subject-id fallback
//   2. BangumiClient episode metadata (/v0/episodes/{id})
//   3. SourceManager / WebSelectorProvider online scrape
//
// The result is the minimum data set the player needs to start
// playback: episode meta + candidate source URLs + resume position.

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include "net/bgm_types.hpp"
#include "core/source_manager.hpp"

namespace aniswitch {

struct ResolvedEpisode {
    int32_t     episodeId        = 0;   // Bangumi episode id
    int32_t     dandanplayId     = 0;   // dandanplay episode id
    int32_t     bangumiSubjectId = 0;
    std::string title;
    double      episodeNumber    = 0;
    int64_t     durationMs       = 0;
    int64_t     resumePositionMs = 0;
    std::vector<VideoSource> sources;
    // Short provenance / failure hint for the picker & player UI
    // (e.g. "sources.json", "在线解析", "无映射").  Empty if unknown.
    std::string resolveNote;
};

using ResolvedCb = std::function<void(ResolvedEpisode)>;

class EpisodeResolver {
public:
    // Resolve a Bangumi episode into a ResolvedEpisode.
    // - subjectId:        Bangumi subject id (fallback key + meta)
    // - episodeId:        Bangumi episode id  (sources.json primary key)
    // - onResult:         called with the resolved result.  May be called
    //                     with sources.empty() when every path was tried
    //                     but nothing playable was found — callers should
    //                     treat that as "no sources", not "parse failed".
    // - onError:          called only on hard failures (invalid id,
    //                     network/HTTP error, Bangumi 404) with a
    //                     classified Chinese message.
    static void resolve(int32_t subjectId,
                        int32_t episodeId,
                        ResolvedCb onResult,
                        std::function<void(const std::string&, int)> onError);

    // Set the WebSelectorProvider pointer used to push per-episode
    // context (subject name, episode sort) before the resolver chain
    // asks SourceManager to enumerate sources.  Pointer is borrowed
    // (SourceManager owns the provider).
    static void setWebSelectorProvider(class WebSelectorProvider* p);
};

}  // namespace aniswitch
