// SPDX-License-Identifier: AGPL-3.0
//
// Bridges between our network-layer danmaku (ParsedDanmaku, from
// dandanplay JSON/XML and from myani REST) and the player-layer
// DanmakuCore (which wants wiliwili-shaped DanmakuItem objects).
//
// Historical danmaku is loaded via loadHistorical(); the renderer
// sorts + dedupes and pushes into DanmakuCore.  Live danmaku (i.e.
// danmaku arriving after the video started) is the caller's job —
// the renderer deliberately has no push subscriber because both
// upstream sources (dandanplay, myani) are pull-based.  See
// player_activity.cpp for the orchestration loop.

#pragma once

#include <vector>
#include "net/danmaku_parser.hpp"
#include "player/danmaku_core.hpp"

namespace aniswitch {

class DanmakuRenderer {
public:
    static DanmakuRenderer& instance();

    // Load a list of historical danmaku for the current episode.
    // Duplicates by play time are dropped (first wins) to keep the
    // timeline clean when merging dandanplay + myani sources.
    void loadHistorical(const std::vector<ParsedDanmaku>& items);

    // Local send helper: optimistically injects the danmaku into the
    // local DanmakuCore for instant display.  The caller is
    // responsible for actually POSTing it to the upstream source.
    void sendLocal(const std::string& text, int32_t mode = 1,
                   int32_t color = 0xFFFFFF, int32_t fontSize = 25);

    // Clear everything (used on episode change / exit).
    void clear();

private:
    DanmakuRenderer() = default;
    static DanmakuItem toItem(const ParsedDanmaku& p);
};

}  // namespace aniswitch
