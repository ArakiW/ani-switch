// SPDX-License-Identifier: AGPL-3.0
//
// ani-switch PATCH (2026-09-02): minimal stub for the wiliwili bilibili.h
// header. The real file is a B站-specific types module; we don't use B站
// APIs. Anything that needs B站 specifics should replace this with the
// upstream bilibili.h from wiliwili-reference/wiliwili/include/utils/.

#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace aniswitch {

// Placeholder types matching wiliwili bilibili.h's name surface so
// subtitle_core.cpp can compile. The values are never read in our
// stubbed-out subtitle pipeline.
struct SubtitleItem {
    std::string id;
    std::string lan;
    std::string lan_doc;
    std::string subtitle_url;
    std::string author;
};

struct VideoSubtitle {
    std::vector<SubtitleItem> subtitles;
};

using VideoPageResult   = VideoSubtitle;
using VideoPageSubtitle = SubtitleItem;

}  // namespace aniswitch
