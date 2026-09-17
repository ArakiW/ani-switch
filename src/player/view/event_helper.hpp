#pragma once
// TsVitch player port → ani-switch event bridge
#include "utils/event_helper.hpp"
#include <borealis/core/event.hpp>
#include <ctime>
#include <string>
#include <fmt/format.h>

using aniswitch::MpvEventEnum;
using aniswitch::MPVEvent;
using CustomEvent = brls::Event<std::string, void*>;
#define APP_E (&aniswitch::applicationEvent)
#ifndef GA
#define GA(...) do { } while (0)
#endif

namespace tsvitch {
inline time_t unix_time() { return std::time(nullptr); }
inline std::string sec2Time(int64_t sec) {
    if (sec < 0) sec = 0;
    int h = static_cast<int>(sec / 3600);
    int m = static_cast<int>((sec % 3600) / 60);
    int s = static_cast<int>(sec % 60);
    return h > 0 ? fmt::format("{:d}:{:02d}:{:02d}", h, m, s)
                 : fmt::format("{:d}:{:02d}", m, s);
}
}  // namespace tsvitch
