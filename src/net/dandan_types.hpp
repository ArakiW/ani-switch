// SPDX-License-Identifier: AGPL-3.0
//
// Shared dandanplay data types. Both dandanplay_client and danmaku_parser
// reference DandanComment, so we keep it in a leaf header that neither
// pulls cpr.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace aniswitch {

struct DandanEpisode {
    int32_t     episodeId    = 0;
    std::string episodeTitle;
    int32_t     episodeNumber = 0;
    std::string bangumiId;
    std::string bangumiTitle;
    std::string animeTitle;
    int32_t     type           = 0;
    int32_t     shift          = 0;
    std::string source;
    std::string sourceUrl;
};

struct DandanAnime {
    std::string bangumiId;
    std::string animeTitle;
    std::string image;
    int32_t     type           = 0;
    bool        isAiring       = false;
    int32_t     season         = 0;
    int32_t     rating         = 0;
    std::vector<DandanEpisode> episodes;
};

struct DandanMatchResult {
    bool        matched         = false;
    int32_t     episodeId       = 0;
    std::string animeTitle;
    std::string episodeTitle;
    int64_t     shift           = 0;
    int64_t     videoDuration   = 0;
    int32_t     bangumiId       = 0;
    std::vector<DandanEpisode> episodes;
};

struct DandanComment {
    int64_t  time    = 0;
    int32_t  mode    = 1;
    int32_t  fontSize = 25;
    int32_t  color   = 0xFFFFFF;
    std::string author;
    std::string text;
};

// nlohmann::json ADL hooks. These are needed because borealis vendors
// nlohmann/json 3.11.2 (json_abi_v3_11_2) which has stricter SFINAE on
// get<T>() — the system nlohmann on the PC build (3.7.x) used relaxed
// checks and "just worked" without explicit from_json, but the Switch
// build hits "no matching function for get<std::vector<DandanComment>>()"
// without these overloads. The dandanplay API JSON uses snake_case field
// names; we map them to our camelCase struct fields here.
//
// Real dandanplay field names (from https://api.dandanplay.net/swagger/ui/index.html):
//   episode:    episodeId, episodeTitle, episodeNumber, bangumiId,
//               bangumiTitle, animeTitle, type, shift, source, sourceUrl
//   anime:      bangumiId, animeTitle, image, type, isAiring, season,
//               rating, episodes[]
//   match:      isMatched, episodeId, animeTitle, episodeTitle, shift,
//               videoDuration, matchId, bangumiId, episodes[]
//   comment:    cid, p (time), m (text), mode, fontSize, color, ...

inline void from_json(const nlohmann::json& j, DandanEpisode& e) {
    j.at("episodeId").get_to(e.episodeId);
    if (j.contains("episodeTitle")) j.at("episodeTitle").get_to(e.episodeTitle);
    if (j.contains("episodeNumber")) j.at("episodeNumber").get_to(e.episodeNumber);
    if (j.contains("bangumiId")) j.at("bangumiId").get_to(e.bangumiId);
    if (j.contains("bangumiTitle")) j.at("bangumiTitle").get_to(e.bangumiTitle);
    if (j.contains("animeTitle")) j.at("animeTitle").get_to(e.animeTitle);
    if (j.contains("type")) j.at("type").get_to(e.type);
    if (j.contains("shift")) j.at("shift").get_to(e.shift);
    if (j.contains("source")) j.at("source").get_to(e.source);
    if (j.contains("sourceUrl")) j.at("sourceUrl").get_to(e.sourceUrl);
}

inline void from_json(const nlohmann::json& j, DandanAnime& a) {
    if (j.contains("bangumiId")) j.at("bangumiId").get_to(a.bangumiId);
    if (j.contains("animeTitle")) j.at("animeTitle").get_to(a.animeTitle);
    if (j.contains("image")) j.at("image").get_to(a.image);
    if (j.contains("type")) j.at("type").get_to(a.type);
    if (j.contains("isAiring")) j.at("isAiring").get_to(a.isAiring);
    if (j.contains("season")) j.at("season").get_to(a.season);
    if (j.contains("rating")) j.at("rating").get_to(a.rating);
    if (j.contains("episodes")) j.at("episodes").get_to(a.episodes);
}

inline void from_json(const nlohmann::json& j, DandanMatchResult& m) {
    if (j.contains("isMatched")) j.at("isMatched").get_to(m.matched);
    if (j.contains("matched"))  j.at("matched").get_to(m.matched);   // tolerate either key
    if (j.contains("episodeId")) j.at("episodeId").get_to(m.episodeId);
    if (j.contains("animeTitle")) j.at("animeTitle").get_to(m.animeTitle);
    if (j.contains("episodeTitle")) j.at("episodeTitle").get_to(m.episodeTitle);
    if (j.contains("shift")) j.at("shift").get_to(m.shift);
    if (j.contains("videoDuration")) j.at("videoDuration").get_to(m.videoDuration);
    if (j.contains("bangumiId")) j.at("bangumiId").get_to(m.bangumiId);
    if (j.contains("episodes")) j.at("episodes").get_to(m.episodes);
}

inline void from_json(const nlohmann::json& j, DandanComment& c) {
    // dandanplay uses 'p' for play time, 'm' for message text
    if (j.contains("p"))     j.at("p").get_to(c.time);
    if (j.contains("time")) j.at("time").get_to(c.time);   // tolerate either
    if (j.contains("mode"))   j.at("mode").get_to(c.mode);
    if (j.contains("fontSize")) j.at("fontSize").get_to(c.fontSize);
    if (j.contains("color"))  j.at("color").get_to(c.color);
    if (j.contains("author")) j.at("author").get_to(c.author);
    if (j.contains("m"))     j.at("m").get_to(c.text);
    if (j.contains("text")) j.at("text").get_to(c.text);   // tolerate either
    if (j.contains("cid")) (void)j.at("cid");               // we don't track cid
}

}  // namespace aniswitch
