// SPDX-License-Identifier: AGPL-3.0
#include "core/web_selector_cache.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

namespace aniswitch::cache {

std::string sanitizeForPath(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-')
            out.push_back(c);
        else
            out.push_back('_');
        if (out.size() >= 64) break;
    }
    return out;
}

namespace {
std::string slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
}  // namespace

std::string readSearchHtml(const std::string& cacheDir,
                           const std::string& sourceName,
                           const std::string& keyword) {
    if (cacheDir.empty()) return {};
    const std::string sub = sanitizeForPath(sourceName);
    const std::string kwSan = sanitizeForPath(keyword);
    if (sub.empty() || kwSan.empty()) return {};
    return slurp(cacheDir + "/" + sub + "/search_" + kwSan + ".html");
}

std::string readDetailHtml(const std::string& cacheDir,
                           const std::string& sourceName,
                           const std::string& keyword) {
    if (cacheDir.empty()) return {};
    const std::string sub = sanitizeForPath(sourceName);
    const std::string kwSan = sanitizeForPath(keyword);
    if (sub.empty() || kwSan.empty()) return {};
    return slurp(cacheDir + "/" + sub + "/detail_" + kwSan + ".html");
}

}  // namespace aniswitch::cache
