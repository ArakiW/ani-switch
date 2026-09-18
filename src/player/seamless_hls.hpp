// SPDX-License-Identifier: AGPL-3.0
//
// Seamless HLS: one continuous MPEG-TS byte stream for mpv (wiliwili-style
// single demuxer timeline). Segments download via our HTTP stack into a
// growing local file; mpv reads through the custom `ani://` protocol and
// blocks when the reader catches the writer — no playlist boundaries.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aniswitch {

class SeamlessHls {
public:
    // Register `ani://` on an mpv handle (call once after mpvCreate).
    static void registerProtocol(void* mpvHandle);

    // Begin downloading `segUrls` into `cachePath`. Returns session id.
    // URI for mpv: "ani://" + id
    // totalDurationSec: full episode length from m3u8 EXTINF sum (for UI).
    // totalSegs: playlist segment count (progress estimate).
    static std::string start(std::vector<std::string> segUrls,
                             const std::string& cachePath,
                             const std::string& referer = "",
                             int totalDurationSec = 0,
                             int totalSegs = 0);

    // Cancel session (Activity gone / user stop). Safe if already closed.
    static void cancel(const std::string& id);

    static bool isSeamlessUri(const std::string& uri);

    // Stats for OSD / logs.
    static void stats(const std::string& id, size_t* bytesOut, size_t* segsOut,
                      bool* doneOut, int* totalDurationSecOut = nullptr,
                      int* totalSegsOut = nullptr);

    // Parse HLS media playlist: sum #EXTINF durations → seconds.
    static int parsePlaylistDurationSec(const std::string& m3u8Body);
};

}  // namespace aniswitch
