// SPDX-License-Identifier: AGPL-3.0

#include "core/local_video_scanner.hpp"
#include <borealis/core/logger.hpp>
#include <algorithm>
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
