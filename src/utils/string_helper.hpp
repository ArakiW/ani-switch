// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace aniswitch {

// Strip surrounding whitespace.
std::string trim(const std::string& s);

// Split on a single character delimiter.
std::vector<std::string> split(const std::string& s, char delim);

// Case-insensitive contains.
bool icontains(const std::string& haystack, const std::string& needle);

// Safe UTF-8 -> std::string conversion (logs replacement character on bad bytes).
std::string sanitize_utf8(const std::string& in);

// Convert seconds (double) to "MM:SS" or "H:MM:SS".
std::string format_duration(double seconds);

// Convert bytes to "1.2 MB" / "456 KB" style.
std::string format_bytes(int64_t bytes);

}  // namespace aniswitch
