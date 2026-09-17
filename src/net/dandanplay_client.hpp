// SPDX-License-Identifier: AGPL-3.0
//
// dandanplay (api.dandanplay.net) client. We use dandanplay purely as
// a danmaku + (in the v0.1 release) episode-resolution source; for the
// actual video URL we fall through to dandanplay's "match" endpoint and
// the upstream "Bangumi Source Provider" (creamycake-anime/ani-subs
// schema) once we wire it in.
//
// Endpoints used (see https://api.dandanplay.net/swagger/ui/index):
//
//   GET  /api/v2/match                                  file-hash match
//   POST /api/v2/match                                  file-hash + fname match
//   GET  /api/v2/bangumi/{animeId}                      anime meta
//   GET  /api/v2/bangumi/{animeId}/episodes             episode list
//   GET  /api/v2/bangumi/bgmtv/{bgmtvSubjectId}         by bangumi.tv id
//   GET  /api/v2/bangumi/season/anime/{year}/{month}    season
//   GET  /api/v2/search/anime?keyword=...               anime name search
//   GET  /api/v2/search/episodes?anime=...&episode=...  episode text search
//   GET  /api/v2/episodes/{episodeId}                   single episode
//   GET  /api/v2/comment/{episodeId}                    danmaku XML
//   POST /api/v2/match/bypath                           file-path match
//   POST /api/v2/search/episodes                        text search
//
// All v2 endpoints return JSON envelopes of the form { errorCode, errorMessage, success }.
//
// Authentication (animeko's open-ani/animeko client is the reference, see
// danmaku/dandanplay/DandanplayClient.kt):
//   X-AppId:     the app id
//   X-Timestamp: unix seconds at the moment of the request
//   X-Signature: Base64( SHA256( appId + timestamp + path + appSecret ) )
// where `path` is the URL-encoded path of the request, e.g. `/api/v2/match`.

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "net/dandan_types.hpp"
#include "net/http.hpp"

namespace aniswitch {

using AnimeCb         = std::function<void(DandanAnime)>;
using EpisodeListCb   = std::function<void(std::vector<DandanEpisode>)>;
using EpisodeCb       = std::function<void(DandanEpisode)>;
using MatchCb         = std::function<void(DandanMatchResult)>;
using CommentListCb   = std::function<void(std::vector<DandanComment>)>;
using AnimeSearchCb   = std::function<void(std::vector<DandanEpisode>)>;

class DandanplayClient {
public:
    static const std::string& baseUrl();   // "https://api.dandanplay.net"

    // Set the application credentials. Both are required to sign
    // requests. `appSecret` is the value obtained alongside `appId`
    // from dandanplay's developer center; keep both private.
    static void setAppCredentials(const std::string& appId, const std::string& appSecret);
    // Legacy single-credential setter (kept for v0.1 builds that
    // haven't yet migrated to the dual-credential form).
    static void setAppId(const std::string& appId);

    // Whether a full appId+appSecret pair is configured. When false,
    // calls are still allowed but the server will throttle or 403.
    static bool hasFullCredentials();

    // ---- Match ----------------------------------------------------------
    //   match by file path
    static void matchByPath(const std::string& filePath, int64_t fileSize,
                            const std::string& fileHashMd5, int64_t videoDurationMs,
                            MatchCb callback = nullptr,
                            ErrorCallback error = nullptr);

    //   match by exact BangumiId + EpisodeId (use after BangumiClient lookup)
    static void matchByIds(int32_t bangumiId, int32_t episodeNumber,
                           MatchCb callback = nullptr,
                           ErrorCallback error = nullptr);

    //   match by file name (no hash). Mirrors animeko's
    //   DandanplayClient.matchVideo(...). The new dandanplay v2
    //   endpoint expects { fileName, fileSize?, videoDuration,
    //   matchMode="fileNameOnly" } and is more forgiving for renamed
    //   files.
    static void matchByFileName(const std::string& fileName,
                                int64_t fileSize,
                                int64_t videoDurationSec,
                                MatchCb callback = nullptr,
                                ErrorCallback error = nullptr);

    // ---- Anime / episodes -----------------------------------------------
    static void getAnime(const std::string& bangumiId,
                         AnimeCb callback = nullptr,
                         ErrorCallback error = nullptr);

    static void getEpisodes(const std::string& bangumiId,
                            EpisodeListCb callback = nullptr,
                            ErrorCallback error = nullptr);

    static void getEpisode(int32_t episodeId,
                           EpisodeCb callback = nullptr,
                           ErrorCallback error = nullptr);

    // Lookup dandanplay anime by bangumi.tv subject id. Useful as a
    // cross-reference bridge from the Bangumi API to dandanplay's
    // danmaku index.
    static void getAnimeByBangumiSubject(int32_t bangumiSubjectId,
                                         EpisodeListCb callback = nullptr,
                                         ErrorCallback error = nullptr);

    // Season search (dandanplay groups new releases by year+month).
    // Returns anime metadata for the season; per-episode danmaku still
    // requires the per-episode id.
    static void getSeasonAnime(int year, int month,
                               AnimeSearchCb callback = nullptr,
                               ErrorCallback error = nullptr);

    // Anime-name search. Returns the matched anime list with their
    // dandanplay internal ids.
    static void searchAnime(const std::string& animeName,
                            AnimeSearchCb callback = nullptr,
                            ErrorCallback error = nullptr);

    // Episode text search. `episodeName` may be empty to get all
    // episodes for the anime.
    static void searchEpisodesByAnime(const std::string& animeName,
                                      const std::string& episodeName,
                                      EpisodeListCb callback = nullptr,
                                      ErrorCallback error = nullptr);

    // ---- Comments (danmaku) --------------------------------------------
    //   with_related: include comments from related episodes (default true)
    //   ch_convert:   0=none 1=Trad 2=Simplified
    //   mode:         comma-separated modes to include (default "1,2,3,4,5")
    static void getComments(int32_t episodeId,
                            bool withRelated = true,
                            int chConvert = 0,
                            const std::string& mode = "",
                            CommentListCb callback = nullptr,
                            ErrorCallback error = nullptr);

    // ---- Text search ----------------------------------------------------
    static void searchEpisodes(const std::string& keyword,
                               AnimeSearchCb callback = nullptr,
                               ErrorCallback error = nullptr);

    // Raw XML endpoint for danmaku. The dandanplay XML format is
    // identical in shape to the legacy Bilibili danmaku xml, so the
    // danmaku_parser can reuse B站 parsing logic for `<d p="...">...</d>`
    // elements. Returns the raw response body (XML).
    using RawCb = std::function<void(std::string)>;
    static void getCommentsRaw(int32_t episodeId,
                               RawCb callback = nullptr,
                               ErrorCallback error = nullptr);
};

}  // namespace aniswitch
