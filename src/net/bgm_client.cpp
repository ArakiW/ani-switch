// SPDX-License-Identifier: AGPL-3.0
//
// Bangumi API client implementation. Uses the public v0 REST API at
// api.bgm.tv. All requests are non-blocking (cpr's async API).

#include "net/bgm_client.hpp"
#include <cpr/cpr.h>
#include <borealis/core/logger.hpp>
#include <fmt/format.h>
#include <mutex>

namespace aniswitch {

namespace {
    std::string g_token;
    std::mutex g_tokenMutex;
    const std::string BASE = "https://api.bgm.tv";
    const std::string USER_AGENT = "ani-switch/0.1.0 (https://github.com/MiniMax139102/ani-switch)";

    cpr::Header authedHeaders() {
        cpr::Header h = HTTP::HEADERS;
        h["User-Agent"] = USER_AGENT;
        std::lock_guard<std::mutex> lock(g_tokenMutex);
        if (!g_token.empty()) h["Authorization"] = "Bearer " + g_token;
        return h;
    }
}  // namespace

const std::string& BangumiClient::baseUrl() { return BASE; }

void BangumiClient::setAccessToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(g_tokenMutex);
    g_token = token;
}

void AuthedHTTP::authedGet(const std::string& url, cpr::Parameters params,
                           std::function<void(const cpr::Response&)> cb,
                           ErrorCallback err) {
    auto session = HTTP::createSession();
    // v16.10.8.2: pre-resolve hostname so libcurl never calls the
    // broken Switch newlib gethostbyname.  GetCallback routes
    // through cpr's GlobalThreadPool (async), which is not covered
    // by the sync _cpr_get/_cpr_post rewrite — without this
    // helper every bgm.tv request still hits the newlib DNS
    // resolver and hangs.
    std::string effectiveUrl, hostForHeader;
    HTTP::rewriteUrlForIP(url, effectiveUrl, hostForHeader);
    session->SetUrl(cpr::Url{effectiveUrl});
    session->SetParameters(params);
    session->SetHeader(authedHeaders());
    if (!hostForHeader.empty()) {
        session->UpdateHeader(cpr::Header{{"Host", hostForHeader}});
    }
    session->GetCallback([cb, err](const cpr::Response& r) {
        if (r.error) { fireError(err, r.error.message, -1); return; }
        if (r.status_code == 401) {
            fireError(err, "Unauthorized �?token may be expired", 401);
            return;
        }
        if (r.status_code != 200) {
            fireError(err, fmt::format("HTTP {}", r.status_code), r.status_code);
            return;
        }
        cb(r);
    });
}

void AuthedHTTP::authedPost(const std::string& url, cpr::Parameters params,
                            nlohmann::json payload,
                            std::function<void(const cpr::Response&)> cb,
                            ErrorCallback err) {
    auto session = HTTP::createSession();
    // v16.10.8.2: pre-resolve DNS — see authedGet above.
    std::string effectiveUrl, hostForHeader;
    HTTP::rewriteUrlForIP(url, effectiveUrl, hostForHeader);
    session->SetUrl(cpr::Url{effectiveUrl});
    session->SetParameters(params);
    session->SetBody(cpr::Body{payload.dump()});
    auto headers = authedHeaders();
    headers["Content-Type"] = "application/json";
    session->SetHeader(headers);
    if (!hostForHeader.empty()) {
        session->UpdateHeader(cpr::Header{{"Host", hostForHeader}});
    }
    session->PostCallback([cb, err](const cpr::Response& r) {
        if (r.error) { fireError(err, r.error.message, -1); return; }
        if (r.status_code == 401) {
            fireError(err, "Unauthorized �?token may be expired", 401);
            return;
        }
        if (r.status_code != 200 && r.status_code != 204) {
            fireError(err, fmt::format("HTTP {}", r.status_code), r.status_code);
            return;
        }
        cb(r);
    });
}

void BangumiClient::getSubject(int32_t subjectId, SubjectCb callback, ErrorCallback error) {
    AuthedHTTP::authedGet(BASE + fmt::format("/v0/subjects/{}", subjectId), {},
        [callback, error](const cpr::Response& r) {
            HTTP::parseJson<Subject>(r, callback, error);
        }, error);
}

void BangumiClient::getEpisodes(int32_t subjectId, int32_t type,
                                EpisodeListCb callback, ErrorCallback error) {
    cpr::Parameters params = {
        {"subject_id", std::to_string(subjectId)},
        {"type",   std::to_string(type)},
        {"limit",  "100"},
        {"offset", "0"},
    };
    AuthedHTTP::authedGet(BASE + "/v0/episodes", params,
        [callback, error](const cpr::Response& r) {
            HTTP::parseJson<std::vector<Episode>>(r, callback, error);
        }, error);
}

void BangumiClient::getPersons(int32_t subjectId, PersonListCb callback, ErrorCallback error) {
    AuthedHTTP::authedGet(BASE + fmt::format("/v0/subjects/{}/persons", subjectId), {},
        [callback, error](const cpr::Response& r) {
            HTTP::parseJson<std::vector<Person>>(r, callback, error);
        }, error);
}

void BangumiClient::getRelations(int32_t subjectId, RelationListCb callback, ErrorCallback error) {
    AuthedHTTP::authedGet(BASE + fmt::format("/v0/subjects/{}/subjects", subjectId), {},
        [callback, error](const cpr::Response& r) {
            HTTP::parseJson<std::vector<SubjectRelation>>(r, callback, error);
        }, error);
}

void BangumiClient::getCharacters(int32_t subjectId, CharacterListCb callback, ErrorCallback error) {
    AuthedHTTP::authedGet(BASE + fmt::format("/v0/subjects/{}/characters", subjectId), {},
        [callback, error](const cpr::Response& r) {
            HTTP::parseJson<std::vector<SubjectCharacter>>(r, callback, error);
        }, error);
}

void BangumiClient::getCalendar(CalendarCb callback, ErrorCallback error) {
    HTTP::getResult<nlohmann::json>(BASE + "/calendar", {},
        [callback](nlohmann::json groups) {
            std::vector<CalendarItem> result;
            for (const auto& group : groups) {
                int weekday = group.at("weekday").at("id").get<int>() % 7;
                for (const auto& item : group.at("items"))
                    result.push_back({weekday, item.get<Subject>()});
            }
            if (callback) callback(std::move(result));
        }, error);
}

void BangumiClient::getPerson(int32_t personId, PersonDetailCb callback, ErrorCallback error) {
    if (personId <= 0) {
        if (error) error("invalid personId", -1);
        return;
    }
    AuthedHTTP::authedGet(BASE + "/v0/persons/" + std::to_string(personId), {},
        [callback, error](const cpr::Response& r) {
            HTTP::parseJson<Person>(r, callback, error);
        }, error);
}

void BangumiClient::getPersonSubjects(int32_t personId, int limit,
                                      PersonSubjectListCb callback, ErrorCallback error) {
    if (personId <= 0) {
        if (error) error("invalid personId", -1);
        return;
    }
    AuthedHTTP::authedGet(BASE + "/v0/persons/" + std::to_string(personId) + "/subjects",
        {{"limit", std::to_string(limit)}, {"offset", "0"}},
        [callback, error](const cpr::Response& r) {
            std::vector<SearchSubject> list;
            if (r.status_code == 200) {
                try {
                    auto j = nlohmann::json::parse(r.text);
                    if (j.is_array()) {
                        for (const auto& it : j) list.push_back(it.get<SearchSubject>());
                    }
                } catch (const std::exception& e) {
                    if (error) error(e.what(), r.status_code);
                    return;
                }
            }
            if (callback) callback(std::move(list));
        }, error);
}

void BangumiClient::getSubjectComments(int32_t subjectId, int limit, int offset,
                                       CommentListCb callback, ErrorCallback error) {
    if (subjectId <= 0) {
        if (error) error("invalid subjectId", -1);
        return;
    }
    AuthedHTTP::authedGet(BASE + "/v0/subjects/" + std::to_string(subjectId) + "/comments",
        {{"limit", std::to_string(limit)}, {"offset", std::to_string(offset)}},
        [callback, error](const cpr::Response& r) {
            std::vector<Comment> list;
            if (r.status_code == 200) {
                try {
                    auto j = nlohmann::json::parse(r.text);
                    if (j.is_object() && j.contains("data") && j["data"].is_array()) {
                        for (const auto& it : j["data"]) list.push_back(it.get<Comment>());
                    } else if (j.is_array()) {
                        for (const auto& it : j) list.push_back(it.get<Comment>());
                    }
                } catch (const std::exception& e) {
                    if (error) error(e.what(), r.status_code);
                    return;
                }
            }
            if (callback) callback(std::move(list));
        }, error);
}

// v17.2: aggregate rating.  Public, no auth.
void BangumiClient::getSubjectRating(int32_t subjectId,
                                    RatingCb callback, ErrorCallback error) {
    if (subjectId <= 0) {
        if (error) error("invalid subjectId", -1);
        return;
    }
    // Use plain HTTP::getResult (not authed) — rating is public.
    HTTP::getResult<SubjectRating>(
        BASE + "/v0/subjects/" + std::to_string(subjectId) + "/rating",
        cpr::Parameters{},
        [callback, error](SubjectRating r) {
            if (callback) callback(std::move(r));
        },
        [callback, error](const std::string& message, int code) {
            // parseJson fired its error path.  Bangumi returns 404
            // for unrated subjects — treat that as "empty rating"
            // (zero scores, no rank) rather than an error.
            if (code == 404 && callback) {
                callback(SubjectRating{});
            } else if (error) {
                error(message, code);
            }
        });
}

// v17.2: current user's collection status for a single subject.
void BangumiClient::getMyCollectionStatus(int32_t subjectId,
                                           CollectionCb callback, ErrorCallback error) {
    if (subjectId <= 0) {
        if (error) error("invalid subjectId", -1);
        return;
    }
    // v0/users/{me}/collections/{subject_id} requires the
    // numeric user_id (ProgramConfig::getUserID()) as `me`,
    // not the username.  The "me" alias is what Bangumi accepts.
    AuthedHTTP::authedGet(
        BASE + fmt::format("/v0/users/me/collections/{}", subjectId),
        cpr::Parameters{},
        [callback, error](const cpr::Response& r) {
            if (r.status_code == 200) {
                HTTP::parseJson<UserCollection>(r, callback, error);
            } else if (r.status_code == 404) {
                // Subject is not in the user's collection.
                if (callback) callback(UserCollection{});
            } else {
                if (error) error("collection HTTP " + std::to_string(r.status_code),
                                  r.status_code);
            }
        }, error);
}

void BangumiClient::searchSubjects(const std::string& keyword, int limit, int offset,
                                   const std::string& sort,
                                   SearchCb callback, ErrorCallback error) {
    cpr::Parameters params = {
        {"limit",   std::to_string(limit)},
        {"offset",  std::to_string(offset)},
    };
    nlohmann::json payload = {{"keyword", keyword}, {"sort", sort}, {"filter", {{"type", {2}}}}};
    AuthedHTTP::authedPost(BASE + "/v0/search/subjects", params, payload,
        [callback, error](const cpr::Response& r) {
            HTTP::parseJson<SearchSubjectList>(r, callback, error);
        }, error);
}

void BangumiClient::getUserCollection(const std::string& username, int32_t type,
                                      int limit, CollectionListCb callback, ErrorCallback error) {
    cpr::Parameters params = {
        {"limit",  std::to_string(limit)},
        {"offset", "0"},
    };
    if (type > 0) {
        params.Add({"type", std::to_string(type)});
    }
    AuthedHTTP::authedGet(
        BASE + fmt::format("/v0/users/{}/collections", username), params,
        [callback, error](const cpr::Response& r) {
            HTTP::parseJson<std::vector<UserCollection>>(r, callback, error);
        }, error);
}

void BangumiClient::updateUserCollection(const std::string& username,
                                         int32_t subjectId, int32_t type, int32_t rate,
                                         const std::string& comment, int32_t epStatus,
                                         bool private_, VoidCb callback, ErrorCallback error) {
    nlohmann::json payload = {
        {"type", type}, {"rate", rate}, {"comment", comment}, {"private", private_},
    };
    AuthedHTTP::authedPost(
        BASE + fmt::format("/v0/users/-/collections/{}", subjectId),
        cpr::Parameters{}, payload,
        [callback](const cpr::Response&) { if (callback) callback(); }, error);
}

}  // namespace aniswitch
