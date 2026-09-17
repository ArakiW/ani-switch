// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
#include "utils/string_helper.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <fmt/format.h>

namespace aniswitch {

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == delim) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

bool icontains(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); }
    );
    return it != haystack.end();
}

std::string sanitize_utf8(const std::string& in) {
    // Pass through; if we wanted strict UTF-8 validation we'd use iconv.
    // Switches and modern systems accept raw UTF-8, so we keep it simple.
    return in;
}

std::string format_duration(double seconds) {
    if (seconds < 0) seconds = 0;
    int total = static_cast<int>(seconds);
    int h = total / 3600;
    int m = (total % 3600) / 60;
    int s = total % 60;
    if (h > 0) {
        return fmt::format("{}:{:02d}:{:02d}", h, m, s);
    }
    return fmt::format("{}:{:02d}", m, s);
}

std::string format_bytes(int64_t bytes) {
    if (bytes < 0) return "0 B";
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int u = 0;
    double v = static_cast<double>(bytes);
    while (v >= 1024.0 && u < 4) {
        v /= 1024.0;
        u++;
    }
    if (u == 0) return fmt::format("{} B", bytes);
    return fmt::format("{:.2f} {}", v, units[u]);
}

}  // namespace aniswitch
