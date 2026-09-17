// SPDX-License-Identifier: AGPL-3.0
//
// Convert dandanplay's danmaku JSON (or the legacy XML format) into the
// DanmakuItem structure consumed by player::DanmakuCore.
//
// The DanmakuItem layout is byte-compatible with the wiliwili one, so
// this file is essentially the data-format adapter for the
// DanmakuCore::loadDanmakuData() entry point.

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "net/dandan_types.hpp"

namespace aniswitch {

// Reduced mirror of player::DanmakuItem that the parser produces and
// the player layer will then convert into the final DanmakuItem.
struct ParsedDanmaku {
    int64_t  time      = 0;     // ms from video start
    int32_t  mode      = 1;     // 1=scroll 4=bottom 5=top 7=advanced
    int32_t  fontSize  = 25;
    int32_t  color     = 0xFFFFFF;
    std::string text;
};

// Parse dandanplay's JSON danmaku (vector<DandanComment>) into the
// reduced parser format.
std::vector<ParsedDanmaku> parseDandanJson(const std::vector<DandanComment>& in);

// Parse dandanplay's legacy XML format. The dandanplay XML is shaped
// like Bilibili's old <i><d p="time,mode,fontsize,color,...">text</d></i>.
// Returns an empty vector on any parse failure.
std::vector<ParsedDanmaku> parseDandanXml(const std::string& xml);

}  // namespace aniswitch
