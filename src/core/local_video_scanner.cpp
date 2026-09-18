// SPDX-License-Identifier: AGPL-3.0

#include "core/local_video_scanner.hpp"
#include <borealis/core/logger.hpp>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <system_error>

namespace aniswitch {

namespace {

// C++17 <filesystem> on Switch newlib doesn't accept the sdmc:
// scheme as a path; strip the scheme so the underlying fopen can
// resolve the mount.  We restore the user-facing "sdmc:/" form
// when populating LocalVideoEntry::path.
std::string stripScheme(const std::string& s) {
    if (s.rfind("sdmc:", 0) == 0) {
        return "/" + s.substr(5);
    }
    return s;
}

std::string addScheme(const std::string& nativePath) {
    // Newlib's stat() returns paths that look like
    //   /switch/aniswitch/videos/foo.mp4
    // The mpv property accepts both /switch/... and sdmc:/switch/...;
    // we use the sdmc:/ form so the path round-trips through
    // std::filesystem::path on PC test runs.
    if (nativePath.empty() || nativePath[0] != '/') return nativePath;
    return "sdmc:" + nativePath;
}

std::string trimCopy(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '.' || s[b] == '_'))
        ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '.' ||
                     s[e - 1] == '_'))
        --e;
    return s.substr(b, e - b);
}

// UTF-8 CJK brackets — cannot use char literals (multi-byte).
inline bool startsWithCjkOpen(const std::string& s) {
    return s.size() >= 3 &&
           static_cast<unsigned char>(s[0]) == 0xE3 &&
           static_cast<unsigned char>(s[1]) == 0x80 &&
           static_cast<unsigned char>(s[2]) == 0x90;  // 【
}

// Strip one leading bracket group: "[Group] Rest" / "【字幕组】Rest".
void stripLeadingBracketGroup(std::string& s) {
    while (!s.empty()) {
        size_t closePos = std::string::npos;
        size_t closeLen = 0;
        if (s[0] == '[') {
            closePos = s.find(']', 1);
            closeLen = 1;
        } else if (startsWithCjkOpen(s)) {
            closePos = s.find("】", 3);
            closeLen = 3;
        } else {
            break;
        }
        if (closePos == std::string::npos) break;
        s = s.substr(closePos + closeLen);
        while (!s.empty() && (s[0] == ' ' || s[0] == '-' || s[0] == '_' ||
                              s[0] == '.'))
            s.erase(0, 1);
    }
}

// Filename heuristic (b): prefix before EP/ep/第/ - /【/[.
// "ep" only counts when followed by a digit (avoid "Deep Blue" → "D").
// Marker at pos 0 → empty prefix → 未分类 (e.g. "ep01.mkv").
// No marker at all → whole stem as series key.
std::string seriesFromFilename(const std::string& filename) {
    std::string stem = filename;
    const auto dot = stem.find_last_of('.');
    if (dot != std::string::npos && dot > 0) stem = stem.substr(0, dot);
    stripLeadingBracketGroup(stem);
    if (stem.empty()) return "未分类";

    std::string lower = stem;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    size_t bestPos = std::string::npos;
    bool sawMarker = false;
    auto consider = [&](size_t pos) {
        if (pos == std::string::npos) return;
        sawMarker = true;
        if (pos == 0) {
            // Marker at start → empty series prefix.
            if (bestPos == std::string::npos || 0 < bestPos) bestPos = 0;
            return;
        }
        if (bestPos == std::string::npos || pos < bestPos) bestPos = pos;
    };

    // ep + digit (case-insensitive)
    for (size_t i = 0; i + 2 < lower.size(); ++i) {
        if (lower[i] == 'e' && lower[i + 1] == 'p' &&
            lower[i + 2] >= '0' && lower[i + 2] <= '9') {
            consider(i);
            break;
        }
    }
    consider(stem.find("第"));
    consider(stem.find(" - "));
    consider(stem.find("【"));
    consider(stem.find('['));

    if (sawMarker) {
        if (bestPos == 0) return "未分类";
        auto prefix = trimCopy(stem.substr(0, bestPos));
        return prefix.empty() ? std::string("未分类") : prefix;
    }
    // No marker — whole stem is the best available series key.
    auto whole = trimCopy(stem);
    return whole.empty() ? "未分类" : whole;
}

// (a) first folder component under the scan root, else (b)/(c).
std::string seriesFromPath(const std::string& nativeFile,
                           const std::string& nativeRoot,
                           const std::string& filename) {
    std::string root = nativeRoot;
    while (!root.empty() && (root.back() == '/' || root.back() == '\\'))
        root.pop_back();
    if (root.empty() || nativeFile.size() <= root.size() + 1)
        return seriesFromFilename(filename);
    if (nativeFile.compare(0, root.size(), root) != 0)
        return seriesFromFilename(filename);
    if (nativeFile[root.size()] != '/' && nativeFile[root.size()] != '\\')
        return seriesFromFilename(filename);

    const std::string rel = nativeFile.substr(root.size() + 1);
    const auto sep = rel.find_first_of("/\\");
    if (sep == std::string::npos || sep == 0)
        return seriesFromFilename(filename);  // file sits in the root
    const auto folder = trimCopy(rel.substr(0, sep));
    return folder.empty() ? seriesFromFilename(filename) : folder;
}

}  // namespace

bool LocalVideoScanner::isPlayableVideo(const std::string& filename) {
    auto lower = filename;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    static const char* kExts[] = {
        ".mp4", ".mkv", ".webm", ".avi", ".m4v", ".mov", ".flv", ".ts", ".m2ts",
    };
    for (auto* ext : kExts) {
        auto len = std::strlen(ext);
        if (lower.size() >= len &&
            lower.compare(lower.size() - len, len, ext) == 0) {
            return true;
        }
    }
    return false;
}

std::vector<LocalVideoEntry> LocalVideoScanner::scanRoot(
    const std::string& root, const std::string& sourceLabel, bool recursive) {
    std::vector<LocalVideoEntry> out;
    std::error_code ec;
    auto nativeRoot = stripScheme(root);
    std::filesystem::path fsRoot(nativeRoot);
    if (!std::filesystem::exists(fsRoot, ec)) return out;
    if (!std::filesystem::is_directory(fsRoot, ec)) return out;

    auto handle = [&](const std::filesystem::directory_entry& entry) {
        if (!entry.is_regular_file(ec)) return;
        auto path = entry.path();
        auto filename = path.filename().string();
        if (!isPlayableVideo(filename)) return;
        LocalVideoEntry e;
        e.path       = addScheme(path.string());
        e.filename   = filename;
        e.source     = sourceLabel;
        auto sz      = entry.file_size(ec);
        e.sizeBytes  = ec ? 0 : static_cast<int64_t>(sz);
        auto ftime   = entry.last_write_time(ec);
        // std::filesystem::file_time_type is platform-specific.
        // On newlib it counts from an arbitrary epoch; we still
        // expose it for sort ordering and just leave the absolute
        // value to whatever newlib gives us.  The activity only
        // uses it for relative comparisons.
        e.mtimeUnix  = ec ? 0 : static_cast<int64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                ftime.time_since_epoch()).count());
        // v22: series key — subfolder under root, else filename heuristic.
        // path here is still native (no sdmc:); root is nativeRoot.
        e.series     = seriesFromPath(path.string(), nativeRoot, filename);
        out.push_back(std::move(e));
    };

    if (recursive) {
        std::error_code recEc;
        for (auto it = std::filesystem::recursive_directory_iterator(
                 fsRoot, recEc);
             it != std::filesystem::recursive_directory_iterator();
             it.increment(recEc)) {
            if (recEc) break;
            handle(*it);
        }
    } else {
        for (auto it = std::filesystem::directory_iterator(fsRoot, ec);
             !ec && it != std::filesystem::directory_iterator();
             it.increment(ec)) {
            handle(*it);
        }
    }
    return out;
}

std::vector<LocalVideoEntry> LocalVideoScanner::scan() {
    std::vector<LocalVideoEntry> all;

    // v20.17: only scan the app drop folder.  Walking sdmc:/ and
    // sdmc:/movies on device crashed / hung (recursive_iterator
    // + newlib + huge cards).  Users put files in videos/.
    try {
        auto a = scanRoot("sdmc:/switch/aniswitch/videos", "videos/", true);
        all.insert(all.end(), a.begin(), a.end());
    } catch (const std::exception& e) {
        brls::Logger::error("LocalVideoScanner: videos scan: {}", e.what());
    } catch (...) {
        brls::Logger::error("LocalVideoScanner: videos scan unknown error");
    }

    // Most recent first.
    std::sort(all.begin(), all.end(),
              [](const LocalVideoEntry& l, const LocalVideoEntry& r) {
                  if (l.mtimeUnix != r.mtimeUnix)
                      return l.mtimeUnix > r.mtimeUnix;
                  return l.filename < r.filename;
              });
    brls::Logger::debug("LocalVideoScanner::scan: {} entries", all.size());
    return all;
}

}  // namespace aniswitch
