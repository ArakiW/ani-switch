// SPDX-License-Identifier: AGPL-3.0
//
// Bangumi.tv (https://api.bgm.tv) client. The public v0 API at
// https://bangumi.github.io/api/ is documented there; we only consume
// the endpoints we need:
//
//   GET  /v0/subjects/{id}             Subject + collection counts
//   GET  /v0/subjects/{id}/episodes    List of Episode (with sort=0..)
//   GET  /v0/subjects/{id}/persons     Cast + staff
//   GET  /v0/subjects/{id}/relations   Sequel / prequel etc.
//   GET  /v0/subjects/{id}/characters  Character + actor mapping
//   GET  /v0/calendar                 Daily broadcast schedule
//   GET  /v0/search/subjects          Subject search
//   GET  /v0/users/{name}/collections User collection list
//   POST /v0/users/{name}/collections Add or update a collection entry
//   PATCH ...                          (same path)
//
// All requests are JSON, all responses follow { request, code, message,
// result }. The HTTP wrapper extracts `result` so the callbacks receive
// the inner object directly.

#pragma once

#include <functional>
#include <string>
#include <vector>
#include "net/bgm_types.hpp"
#include "net/http.hpp"

namespace aniswitch {

using SubjectCb         = std::function<void(Subject)>;
using EpisodeListCb     = std::function<void(std::vector<Episode>)>;
using PersonListCb      = std::function<void(std::vector<Person>)>;
using RelationListCb    = std::function<void(std::vector<SubjectRelation>)>;
using CharacterListCb   = std::function<void(std::vector<SubjectCharacter>)>;
using CalendarCb        = std::function<void(std::vector<CalendarItem>)>;
using SearchCb          = std::function<void(SearchSubjectList)>;
using CollectionListCb  = std::function<void(std::vector<UserCollection>)>;
using VoidCb            = std::function<void()>;

class BangumiClient {
public:
    // The user must hold the access token (loaded from config_helper) and
    // pass it in. The token is sent as a Bearer header; on 401 we ask
    // the auth layer to refresh.
    static void setAccessToken(const std::string& token);

    static const std::string& baseUrl();   // "https://api.bgm.tv"

    // ---- Subject --------------------------------------------------------
    static void getSubject(int32_t subjectId,
                           SubjectCb callback = nullptr,
                           ErrorCallback error = nullptr);

    static void getEpisodes(int32_t subjectId, int32_t type = 0,
                            EpisodeListCb callback = nullptr,
                            ErrorCallback error = nullptr);

    static void getPersons(int32_t subjectId,
                           PersonListCb callback = nullptr,
                           ErrorCallback error = nullptr);

    static void getRelations(int32_t subjectId,
                             RelationListCb callback = nullptr,
                             ErrorCallback error = nullptr);

    static void getCharacters(int32_t subjectId,
                              CharacterListCb callback = nullptr,
                              ErrorCallback error = nullptr);

    // ---- Person (staff / cast) --------------------------------------------
    // v0/persons/{id}            Person detail (bio + image + type)
    // v0/persons/{id}/subjects    Filmography (subjects this person appeared in)
    using PersonDetailCb       = std::function<void(Person)>;
    using PersonSubjectListCb  = std::function<void(std::vector<SearchSubject>)>;
    static void getPerson(int32_t personId,
                          PersonDetailCb callback = nullptr,
                          ErrorCallback error = nullptr);
    static void getPersonSubjects(int32_t personId,
                                  int limit = 30,
                                  PersonSubjectListCb callback = nullptr,
                                  ErrorCallback error = nullptr);

    // ---- Subject comments (v0/subjects/{id}/comments) ---------------------
    // Paged: use `offset` for the next page.  Bangumi returns the
    // most recent first.
    using CommentListCb = std::function<void(std::vector<Comment>)>;
    static void getSubjectComments(int32_t subjectId,
                                   int limit = 20,
                                   int offset = 0,
                                   CommentListCb callback = nullptr,
                                   ErrorCallback error = nullptr);

    // v17.2: aggregate rating for a subject (公开 endpoint,
    // no auth).  v0/subjects/{id}/rating returns
    // {rank, total, count[10], score}.  Empty payload means
    // the subject has not been rated yet.
    using RatingCb = std::function<void(SubjectRating)>;
    static void getSubjectRating(int32_t subjectId,
                                 RatingCb callback = nullptr,
                                 ErrorCallback error = nullptr);

    // v17.2: current user's collection status for a single
    // subject.  v0/users/{me}/collections/{subject_id} (auth
    // required, calls /v0/users/{userId}/collections/{subjectId}
    // under the hood).  Empty body / 404 means the user has
    // not added this subject to their collection yet; the
    // callback is fired with a default-constructed
    // UserCollection in that case.
    using CollectionCb = std::function<void(UserCollection)>;
    static void getMyCollectionStatus(int32_t subjectId,
                                      CollectionCb callback = nullptr,
                                      ErrorCallback error = nullptr);

    // ---- Calendar -------------------------------------------------------
    // Calendar is the un-auth-required schedule.
    static void getCalendar(CalendarCb callback = nullptr,
                            ErrorCallback error = nullptr);

    // ---- Search ---------------------------------------------------------
    static void searchSubjects(const std::string& keyword,
                               int limit = 25,
                               int offset = 0,
                               const std::string& sort = "match",
                               SearchCb callback = nullptr,
                               ErrorCallback error = nullptr);

    // ---- User collection (requires auth) -------------------------------
    static void getUserCollection(const std::string& username,
                                  int32_t type = 0,  // 0 = all
                                  int limit = 50,
                                  CollectionListCb callback = nullptr,
                                  ErrorCallback error = nullptr);

    // Add or update a collection entry. `subject_type` follows the API:
    // 1=想看 2=在看 3=看过 4=搁置 5=抛弃
    static void updateUserCollection(const std::string& username,
                                    int32_t subjectId,
                                    int32_t type,
                                    int32_t rate = 0,
                                    const std::string& comment = "",
                                    int32_t epStatus = 0,
                                    bool private_ = false,
                                    VoidCb callback = nullptr,
                                    ErrorCallback error = nullptr);
};

// Lightweight wrapper that prepends the bearer token to the request.
class AuthedHTTP {
public:
    static void authedGet(const std::string& url, cpr::Parameters params,
                          std::function<void(const cpr::Response&)> cb,
                          ErrorCallback err);
    static void authedPost(const std::string& url, cpr::Parameters params,
                           nlohmann::json payload,
                           std::function<void(const cpr::Response&)> cb,
                           ErrorCallback err);
};

}  // namespace aniswitch
