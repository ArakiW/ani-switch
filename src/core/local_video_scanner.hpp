// SPDX-License-Identifier: AGPL-3.0
//
// v17.4: local video scanner.  Walks a list of well-known directories
// on the SD card and returns all media files for the
// CacheManagementActivity to display.  The supported roots are:
//
//   sdmc:/switch/aniswitch/videos/        <-- user-facing drop folder
//   sdmc:/switch/aniswitch/cache/         <-- episode download cache
//                                              (empty until BT source
//                                              ships in v18+)
//   sdmc:/movies/                         <-- homebrew scene convention
//   sdmc:/                                <-- shallow scan of the SD
//                                              root for stray files
//
// The SD-root scan is bounded to one level (no recursion) so the
// app doesn't get stuck walking a 200 GB card on every launch.
// Result is sorted by modified-time descending so the most recent
// download shows up first.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aniswitch {

struct LocalVideoEntry {
    std::string path;       // absolute, e.g. "sdmc:/switch/aniswitch/videos/ep01.mkv"
    std::string filename;   // basename only, for display
    int64_t     sizeBytes = 0;
    int64_t     mtimeUnix = 0;  // seconds since epoch
    std::string source;     // which root it came from
    // v22: anime/series grouping key for the local video page.
    // Derived in the scanner, priority:
    //   (a) first folder component under the scan root
    //       (videos/葬送的芙莉莲/ep01.mkv → "葬送的芙莉莲")
    //   (b) filename prefix before common markers
    //       (EP/ep/第/ - /【/[) — leading [Group] tags are stripped first
    //   (c) fallback "未分类"
    std::string series;
};

class LocalVideoScanner {
public:
    // Walk all known roots and return matching files.  Pure
    // filesystem I/O — no network, no DB.
    static std::vector<LocalVideoEntry> scan();

    // Walk a single root.  Recursive when `recursive` is true
    // (used for the user-facing drop folder).  Returns an empty
    // vector if the root does not exist or is not readable.
    static std::vector<LocalVideoEntry> scanRoot(const std::string& root,
                                                  const std::string& sourceLabel,
                                                  bool recursive = true);

    // The list of file extensions we treat as playable video.
    // mpv on Switch can decode the common ones; some legacy codecs
    // (RealVideo, WMV) may fail at playback time and surface an
    // mpv error to the user.
    static bool isPlayableVideo(const std::string& filename);
};

}  // namespace aniswitch
