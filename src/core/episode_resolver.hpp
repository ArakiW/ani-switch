// SPDX-License-Identifier: AGPL-3.0
//
// "Episode Resolver" is the high-level helper that ties together:
//   1. BangumiClient to get a Subject's full episode list
//   2. dandanplay match to map a Bangumi episode to a dandanplay id
//   3. SourceManager to enumerate playable URLs for the resolved
//      dandanplay episode
//
// The result is the minimum data set the player needs to start
// playback: danmakuId + source URL + duration + resume position.

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
};

using ResolvedCb = std::function<void(ResolvedEpisode)>;

class EpisodeResolver {
public:
    // Resolve a Bangumi episode into a ResolvedEpisode.
    // - subjectId:        Bangumi subject id (for fallback data)
    // - episodeId:        Bangumi episode id
    // - onResult:         called with the resolved result (sources may be empty)
    // - onError:          called on any failed step
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
