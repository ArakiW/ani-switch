// SPDX-License-Identifier: AGPL-3.0
//
// Plain-data mirror of the JSON schema used by MajoSissi/animeko-source
// (https://github.com/MajoSissi/animeko-source) and compatible forks
// (ani-yuan, css, animeko-prime, etc.).  We deserialize it once at
// startup, cache the in-memory representation, and look up the right
// "web-selector" by the user's subject / episode query.
//
// The schema is documented in the upstream repo; the salient fields are:
//
//   { "exportedMediaSourceDataList": { "mediaSources": [
//     { "factoryId": "web-selector",
//       "version": 2,
//       "arguments": {
//         "name": "酱紫社(修复)",
//         "iconUrl": "...",
//         "searchConfig": {
//           "searchUrl": "...?wd={keyword}",
//           "selectorSubjectFormatA": { "selectLists": "div.x > a" },
//           "selectorChannelFormatFlattened": { ... },
//           "matchVideo": { "matchVideoUrl": "regex", "addHeadersToVideo": { ... } }
//         },
//         "tier": 1
//       }
//     }, ...
//   ] } }
//
// We deliberately accept the upstream field names (snake_case) without
// remapping so the on-disk JSON can be diffed against the upstream repo.

#pragma once

#include <regex>
#include <string>
#include <vector>

namespace aniswitch {

// A single "web selector" media source (one of the entries in
// exportedMediaSourceDataList.mediaSources).  The fields kept here are
// the ones we actually use at runtime:  the human label, the search URL
// template (with `{keyword}` placeholder), the regex used to extract a
// playable m3u8/mp4 URL from a detail page, and the per-request HTTP
// headers (referer / user-agent) the source wants us to send.
struct WebSelectorSource {
    std::string name;        // display name from arguments.name
    std::string iconUrl;     // arguments.iconUrl
    std::string searchUrl;   // searchConfig.searchUrl (with "{keyword}")
    int         tier = 99;   // arguments.tier (lower = higher quality, used to sort)
    int         requestIntervalMs = 3000;  // searchConfig.requestInterval
    std::regex  matchVideoUrl;            // searchConfig.matchVideo.matchVideoUrl
    std::string matchVideoUrlPattern;     // raw pattern; empty = no custom regex
    bool        hasMatchVideoUrl = false; // true only when pattern compiled
    std::string referer;                 // searchConfig.matchVideo.addHeadersToVideo.referer
    std::string userAgent;               // searchConfig.matchVideo.addHeadersToVideo.userAgent
    std::string cookies;                 // searchConfig.matchVideo.cookies (e.g. "quality=1080")

    // The two CSS selectors we actually parse.  See WebSelectorProvider
    // for the extremely small subset of CSS we support.  The full
    // schema has more (channelFormatFlattened, channelFormatNoChannel,
    // etc.) but Switch has no HTML parser / JS engine so we only wire
    // up the simple one-entry list of subjects.
    std::string selectorSubjectA;         // searchConfig.selectorSubjectFormatA.selectLists
    std::string selectorChannelFlattened; // searchConfig.selectorChannelFormatFlattened.selectEpisodeLists
    std::string selectorEpisodesFromList; // ...selectEpisodesFromList
};

// Top-level deserialised shape.  Parse() accepts the upstream JSON
// verbatim; a parse error is reported via Parse()'s return value.
struct WebSelectorData {
    std::vector<WebSelectorSource> sources;
    std::string rawJson;        // for diagnostics

    // Parse a JSON string.  On failure returns false and leaves
    // `sources` empty.
    static bool parse(const std::string& json, WebSelectorData& out);
};

// Built-in default source manifest URL.  The raw.githubusercontent.com
// endpoint is throttled in mainland China, so we go through gh-proxy by
// default; users can override via ProgramConfig::getAnimekoSourceUrl().
constexpr const char* kDefaultAnimekoSourceUrl =
    "https://gh-proxy.com/raw.githubusercontent.com/MajoSissi/animeko-source/main/dist/online.json";

}  // namespace aniswitch
