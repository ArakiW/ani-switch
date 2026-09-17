// SPDX-License-Identifier: AGPL-3.0

#include "net/dandanplay_client.hpp"
#include "net/dandanplay_auth.hpp"
#include <cpr/cpr.h>
#include <borealis/core/logger.hpp>
#include <fmt/format.h>

namespace aniswitch {

namespace {
    const std::string BASE = "https://api.dandanplay.net";
    std::string g_appId;
    std::string g_appSecret;

    // Build the auth headers for a given URL path.  When appSecret is
    // empty we still send X-AppId (helps dandanplay rate-limit per-app)
    // but skip the signature — the server allows unauthenticated calls
    // for the public endpoints but enforces stricter rate limits.
    cpr::Header dpAuthHeaders(const std::string& url) {
        cpr::Header h = HTTP::HEADERS;
        h["User-Agent"] = "ani-switch/0.1.0 (https://github.com/MiniMax139102/ani-switch)";
        h["Accept"]     = "application/json";
        if (g_appId.empty()) return h;
        h["X-AppId"] = g_appId;
        if (g_appSecret.empty()) return h;
        const int64_t ts = dandanplay::nowSeconds();
        h["X-Timestamp"] = std::to_string(ts);
        h["X-Signature"] = dandanplay::sign(g_appId, ts, dandanplay::urlPath(url), g_appSecret);
        return h;
    }

    // dandanplay's "second" timing is very loose (animeko's own
    // reference uses 60s for everything).  We mirror that to keep
    // large season lists from timing out on a slow Switch wifi link.
    const cpr::Timeout DP_TIMEOUT{60000};

    // dandanplay wraps everything in { errorCode, errorMessage, success, ... }.
    // The HTTP wrapper already extracts `result` when present, but the v2
    // envelope uses `errorCode` not `code`. We shim a quick check here.
    bool checkEnvelope(const cpr::Response& r, ErrorCallback err) {
        try {
            auto j = nlohmann::json::parse(r.text);
            if (j.is_object() && j.contains("errorCode")) {
                int code = j["errorCode"].is_number_integer() ? j["errorCode"].get<int>() : 0;
                if (code != 0) {
                    std::string msg = j.contains("errorMessage") && j["errorMessage"].is_string()
                                    ? j["errorMessage"].get<std::string>() : "unknown";
                    fireError(err, msg, code);
                    return false;
                }
            }
            return true;
        } catch (const std::exception& e) {
            fireError(err, e.what(), 200);
            return false;
        }
    }

    template <typename T>
    void dandanParseJson(const cpr::Response& r,
                         std::function<void(T)> callback,
                         ErrorCallback error) {
        if (!checkEnvelope(r, error)) return;
        HTTP::parseJson<T>(r, callback, error);
    }
}  // namespace

const std::string& DandanplayClient::baseUrl() { return BASE; }
void DandanplayClient::setAppCredentials(const std::string& appId, const std::string& appSecret) {
    g_appId = appId;
    g_appSecret = appSecret;
}
void DandanplayClient::setAppId(const std::string& appId) { g_appId = appId; }
bool DandanplayClient::hasFullCredentials() { return !g_appId.empty() && !g_appSecret.empty(); }

static void dandanGet(const std::string& url, cpr::Parameters params,
                      std::function<void(const cpr::Response&)> cb,
                      ErrorCallback err) {
    auto session = HTTP::createSession();
    // v16.10.8.3: pre-resolve DNS — see HTTP::rewriteUrlForIP /
    // AuthedHTTP::authedGet (v16.10.8.2).  Without this, every
    // dandanplay request still hits cpr's newlib gethostbyname
    // and hangs in hbmenu applet mode.
    std::string effectiveUrl, hostForHeader;
    HTTP::rewriteUrlForIP(url, effectiveUrl, hostForHeader);
    session->SetUrl(cpr::Url{effectiveUrl});
    session->SetParameters(params);
    session->SetHeader(dpAuthHeaders(url));
    if (!hostForHeader.empty()) {
        session->UpdateHeader(cpr::Header{{"Host", hostForHeader}});
    }
    session->SetTimeout(DP_TIMEOUT);
    session->GetCallback([cb, err](const cpr::Response& r) {
        if (r.error) { fireError(err, r.error.message, -1); return; }
        if (r.status_code != 200) {
            fireError(err, fmt::format("HTTP {}", r.status_code), r.status_code);
            return;
        }
        cb(r);
    });
}

static void dandanPost(const std::string& url, cpr::Payload payload,
                       std::function<void(const cpr::Response&)> cb,
                       ErrorCallback err) {
    auto session = HTTP::createSession();
    // v16.10.8.3: pre-resolve DNS — see dandanGet above.
    std::string effectiveUrl, hostForHeader;
    HTTP::rewriteUrlForIP(url, effectiveUrl, hostForHeader);
    session->SetUrl(cpr::Url{effectiveUrl});
    session->SetPayload(payload);
    session->SetHeader(dpAuthHeaders(url));
    if (!hostForHeader.empty()) {
        session->UpdateHeader(cpr::Header{{"Host", hostForHeader}});
    }
    session->SetTimeout(DP_TIMEOUT);
    session->PostCallback([cb, err](const cpr::Response& r) {
        if (r.error) { fireError(err, r.error.message, -1); return; }
        if (r.status_code != 200) {
            fireError(err, fmt::format("HTTP {}", r.status_code), r.status_code);
            return;
        }
        cb(r);
    });
}

void DandanplayClient::matchByPath(const std::string& filePath, int64_t fileSize,
                                   const std::string& fileHashMd5, int64_t videoDurationMs,
                                   MatchCb callback, ErrorCallback error) {
    cpr::Payload payload = {
        {"file",         filePath},
        {"fileSize",     std::to_string(fileSize)},
        {"fileHash",     fileHashMd5},
        {"videoDuration", std::to_string(videoDurationMs)},
    };
    dandanPost(BASE + "/api/v2/match", payload,
        [callback, error](const cpr::Response& r) {
            dandanParseJson<DandanMatchResult>(r, callback, error);
        }, error);
}

void DandanplayClient::matchByIds(int32_t bangumiId, int32_t episodeNumber,
                                  MatchCb callback, ErrorCallback error) {
    cpr::Parameters params = {
        {"bangumiId",  std::to_string(bangumiId)},
        {"episode",    std::to_string(episodeNumber)},
    };
    dandanGet(BASE + "/api/v2/match", params,
        [callback, error](const cpr::Response& r) {
            dandanParseJson<DandanMatchResult>(r, callback, error);
        }, error);
}

void DandanplayClient::matchByFileName(const std::string& fileName,
                                       int64_t fileSize, int64_t videoDurationSec,
                                       MatchCb callback, ErrorCallback error) {
    // animeko sends videoDuration.inWholeSeconds and always sets
    // matchMode = "fileNameOnly" (no hash).  fileSize is sent when
    // known (animeko's Kotlin code only puts it in the body when the
    // caller supplied it).
    cpr::Payload payload = {
        {"fileName",       fileName},
        {"videoDuration",  std::to_string(videoDurationSec)},
        {"matchMode",      "fileNameOnly"},
    };
    if (fileSize > 0) payload.Add({"fileSize", std::to_string(fileSize)});
    dandanPost(BASE + "/api/v2/match", payload,
        [callback, error](const cpr::Response& r) {
            dandanParseJson<DandanMatchResult>(r, callback, error);
        }, error);
}

void DandanplayClient::getAnime(const std::string& bangumiId,
                                AnimeCb callback, ErrorCallback error) {
    dandanGet(BASE + "/api/v2/bangumi/" + bangumiId, {},
        [callback, error](const cpr::Response& r) {
            dandanParseJson<DandanAnime>(r, callback, error);
        }, error);
}

void DandanplayClient::getEpisodes(const std::string& bangumiId,
                                   EpisodeListCb callback, ErrorCallback error) {
    dandanGet(BASE + "/api/v2/bangumi/" + bangumiId + "/episodes", {},
        [callback, error](const cpr::Response& r) {
            dandanParseJson<std::vector<DandanEpisode>>(r, callback, error);
        }, error);
}

void DandanplayClient::getEpisode(int32_t episodeId,
                                  EpisodeCb callback, ErrorCallback error) {
    dandanGet(BASE + fmt::format("/api/v2/episodes/{}", episodeId), {},
        [callback, error](const cpr::Response& r) {
            dandanParseJson<DandanEpisode>(r, callback, error);
        }, error);
}

void DandanplayClient::getAnimeByBangumiSubject(int32_t bangumiSubjectId,
                                                 EpisodeListCb callback, ErrorCallback error) {
    dandanGet(BASE + fmt::format("/api/v2/bangumi/bgmtv/{}", bangumiSubjectId), {},
        [callback, error](const cpr::Response& r) {
            dandanParseJson<std::vector<DandanEpisode>>(r, callback, error);
        }, error);
}

void DandanplayClient::getSeasonAnime(int year, int month,
                                      AnimeSearchCb callback, ErrorCallback error) {
    dandanGet(BASE + fmt::format("/api/v2/bangumi/season/anime/{}/{:02d}", year, month), {},
        [callback, error](const cpr::Response& r) {
            dandanParseJson<std::vector<DandanEpisode>>(r, callback, error);
        }, error);
}

void DandanplayClient::searchAnime(const std::string& animeName,
                                   AnimeSearchCb callback, ErrorCallback error) {
    cpr::Parameters params = { {"keyword", animeName} };
    dandanGet(BASE + "/api/v2/search/anime", params,
        [callback, error](const cpr::Response& r) {
            // 404 means "no match" — animeko's reference returns an
            // empty list in that case instead of an error.
            if (r.status_code == 404) {
                if (callback) callback({});
                return;
            }
            dandanParseJson<std::vector<DandanEpisode>>(r, callback, error);
        }, error);
}

void DandanplayClient::searchEpisodesByAnime(const std::string& animeName,
                                             const std::string& episodeName,
                                             EpisodeListCb callback, ErrorCallback error) {
    cpr::Parameters params = { {"anime", animeName} };
    if (!episodeName.empty()) params.Add({"episode", episodeName});
    dandanGet(BASE + "/api/v2/search/episodes", params,
        [callback, error](const cpr::Response& r) {
            dandanParseJson<std::vector<DandanEpisode>>(r, callback, error);
        }, error);
}

void DandanplayClient::getComments(int32_t episodeId, bool withRelated, int chConvert,
                                   const std::string& mode, CommentListCb callback, ErrorCallback error) {
    cpr::Parameters params = {
        {"with_related", withRelated ? "true" : "false"},
        {"ch_convert",   std::to_string(chConvert)},
    };
    if (!mode.empty()) params.Add({"mode", mode});
    dandanGet(BASE + fmt::format("/api/v2/comment/{}", episodeId), params,
        [callback, error](const cpr::Response& r) {
            dandanParseJson<std::vector<DandanComment>>(r, callback, error);
        }, error);
}

void DandanplayClient::getCommentsRaw(int32_t episodeId, RawCb callback, ErrorCallback error) {
    const std::string url = BASE + fmt::format("/api/v2/comment/{}", episodeId) + ".xml";
    auto session = HTTP::createSession();
    // v16.10.8.3: pre-resolve DNS — see dandanGet above.
    std::string effectiveUrl, hostForHeader;
    HTTP::rewriteUrlForIP(url, effectiveUrl, hostForHeader);
    session->SetUrl(cpr::Url{effectiveUrl});
    session->SetHeader(dpAuthHeaders(url));
    if (!hostForHeader.empty()) {
        session->UpdateHeader(cpr::Header{{"Host", hostForHeader}});
    }
    session->SetTimeout(DP_TIMEOUT);
    session->GetCallback([callback, err = std::move(error)](const cpr::Response& r) {
        if (r.error) { fireError(err, r.error.message, -1); return; }
        if (r.status_code != 200) {
            fireError(err, fmt::format("HTTP {}", r.status_code), r.status_code);
            return;
        }
        if (callback) callback(r.text);
    });
}

void DandanplayClient::searchEpisodes(const std::string& keyword,
                                      AnimeSearchCb callback, ErrorCallback error) {
    cpr::Payload payload = { {"keyword", keyword} };
    dandanPost(BASE + "/api/v2/search/episodes", payload,
        [callback, error](const cpr::Response& r) {
            dandanParseJson<std::vector<DandanEpisode>>(r, callback, error);
        }, error);
}

}  // namespace aniswitch
