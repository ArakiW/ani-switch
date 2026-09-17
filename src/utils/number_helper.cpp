// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
#include "utils/number_helper.hpp"
#include <cstdio>
#include <cmath>
#include <fmt/format.h>

namespace aniswitch {

std::string format_thousands(int64_t n) {
    if (n < 1000) return std::to_string(n);
    std::string s = std::to_string(n);
    int insert_pos = static_cast<int>(s.size()) % 3;
    if (insert_pos == 0) insert_pos = 3;
    std::string out;
    for (int i = 0; i < (int)s.size(); i++) {
        if (i > 0 && i == insert_pos) {
            out.push_back(',');
            insert_pos += 3;
        }
        out.push_back(s[i]);
    }
    return out;
}

std::string format_compact(int64_t n) {
    if (n < 0) n = 0;
    if (n < 1000) return std::to_string(n);
    if (n < 10000)        return fmt::format("{:.1f}k", n / 1000.0);
    if (n < 1000000)      return fmt::format("{}k",   n / 1000);
    if (n < 10000000)     return fmt::format("{:.1f}m", n / 1000000.0);
    if (n < 1000000000)   return fmt::format("{}m",   n / 1000000);
    return fmt::format("{:.1f}b", n / 1000000000.0);
}

std::string format_score(double v) {
    if (std::isnan(v)) return "-";
    if (std::fabs(v - std::floor(v)) < 0.05) {
        return fmt::format("{:.0f}", v);
    }
    return fmt::format("{:.1f}", v);
}

}  // namespace aniswitch
