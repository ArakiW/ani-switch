// SPDX-License-Identifier: AGPL-3.0
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace aniswitch {
struct SubtitleTrack {
    int64_t id = 0;
    std::string title;
    std::string language;
    bool selected = false;
};
class SubtitleCore {
public:
    static std::vector<SubtitleTrack> tracks();
    static void select(int64_t id);
    static void disable();
    static void add(const std::string& path);
};
}
