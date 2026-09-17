// SPDX-License-Identifier: AGPL-3.0
//
// Plain-data types for the myani danmaku relay.  This header is
// deliberately free of cpr / libcurl / borealis so the test harness
// (and any other non-network consumer) can include it cheaply.  The
// actual HTTP client lives in net/myani_client.hpp.

#pragma once

#include <cstdint>
#include <string>
#include "net/danmaku_parser.hpp"   // ParsedDanmaku (the shared neutral type)

namespace aniswitch {

// Mirrors open-ani/animeko's DanmakuLocation.  We use a C++ enum
// instead of the original String-typed wire field to keep call
// sites in switch statements and to surface typos at compile time.
enum class MyaniLocation {
    TOP     = 1,
    BOTTOM  = 4,
    NORMAL  = 2,  // scroll (default)
    ADV     = 3,
};

// Wire shape returned by GET /v1/danmaku/<episodeId>.  playTime is
// stored as milliseconds (already converted from the wire's seconds
// double) so callers can hand it straight to the player layer
// without further arithmetic.
struct MyaniDanmaku {
    int64_t       id         = 0;
    int64_t       playTimeMs = 0;
    int32_t       color      = 0xFFFFFF;
    MyaniLocation location  = MyaniLocation::NORMAL;
    std::string   text;
    std::string   senderId;
};

// Convert a myani record to the player-layer neutral format.  Kept
// explicit (rather than implicit) so future diverging fields are
// easy to handle.
inline ParsedDanmaku toParsed(const MyaniDanmaku& m) {
    ParsedDanmaku p;
    p.time     = m.playTimeMs;
    p.mode     = static_cast<int32_t>(m.location);
    p.color    = m.color;
    p.text     = m.text;
    return p;
}

// Wire string for each MyaniLocation — used when building the JSON
// request body for POST /v1/danmaku/<episodeId>.  Returns the empty
// string for unknown values so a corrupt enum value doesn't crash.
inline const char* myaniLocationToString(MyaniLocation loc) {
    switch (loc) {
        case MyaniLocation::TOP:    return "TOP";
        case MyaniLocation::BOTTOM: return "BOTTOM";
        case MyaniLocation::ADV:    return "ADV";
        case MyaniLocation::NORMAL: return "NORMAL";
    }
    return "";
}

}  // namespace aniswitch
