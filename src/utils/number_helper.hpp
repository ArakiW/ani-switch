// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
#pragma once
#include <string>
#include <cstdint>

namespace aniswitch {

// 12345 -> "12,345"
std::string format_thousands(int64_t n);

// "12.3k" / "1.2m" — used for like/coin/view counts in the UI.
std::string format_compact(int64_t n);

// 8.5 -> "8.5"  (one decimal place, no trailing .0 for integers)
std::string format_score(double v);

}  // namespace aniswitch
