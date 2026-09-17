// SPDX-License-Identifier: AGPL-3.0
//
// Video source abstraction. Each Episode has zero or more candidate
// "sources" (HTTP URLs to actual video files). SourceManager picks the
// best one based on user preferences (preferred quality, mirror
// availability) and asks the player to load it.
//
// HTTP providers supply explicit playback URLs; metadata services are not video sources.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace aniswitch {

enum class SourceKind {
    HTTP,
    BT,
    JELLYFIN,
    EMBY,
    CACHE,
};

struct VideoSource {
    SourceKind kind = SourceKind::HTTP;
    std::string url;                 // playable URL (or magnet / file://)
    int32_t     width      = 0;
    int32_t     height     = 0;
    int32_t     bitrate    = 0;      // kbps
    std::string codec;               // "h264" / "hevc" / "av1"
    int32_t     durationMs = 0;
    std::string label;               // "1080p" / "720p 高清" / etc.
    int32_t     priority    = 0;      // higher = preferred
};

using SourceListCb = std::function<void(std::vector<VideoSource>)>;

class ISourceProvider {
public:
    virtual ~ISourceProvider() = default;
    virtual SourceKind kind() const = 0;
    virtual void enumerate(int32_t episodeId,
                           SourceListCb callback,
                           std::function<void(const std::string&, int)> error) = 0;
};

class SourceManager {
public:
    static SourceManager& instance();

    // Add a provider (e.g. HTTP, BT, cache).
    void registerProvider(std::shared_ptr<ISourceProvider> p);

    // Completes once with all successful providers, or errors if all fail.
    void enumerate(int32_t episodeId,
                   SourceListCb callback,
                   std::function<void(const std::string&, int)> error);

    // Pick the single best source from a list. Honours the user's
    // quality / mirror preferences from ProgramConfig.
    std::shared_ptr<VideoSource> pickBest(const std::vector<VideoSource>& sources);

private:
    SourceManager() = default;
    std::vector<std::shared_ptr<ISourceProvider>> providers_;
};

}  // namespace aniswitch
