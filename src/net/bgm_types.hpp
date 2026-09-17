// SPDX-License-Identifier: AGPL-3.0
//
// Bangumi.tv (bangumi.tv / bgm.tv) data model.
//
// References:
//   https://bangumi.github.io/api/
//   https://github.com/bangumi/server
//
// All public types are NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE-friendly. The
// json_helper.hpp header redefines NLOHMANN_JSON_FROM to skip null /
// missing fields, so a missing JSON key never throws during parse.

#pragma once

#include <cstdint>
#include "net/json_helper.hpp"
#include <string>
#include <vector>

namespace aniswitch {

// ---- Subject (番剧 / 书籍 / 音乐 / 游戏 / 影视) -------------------------

struct SubjectImage {
    std::string large;
    std::string common;
    std::string medium;
    std::string small;
    std::string grid;
};

struct SubjectRating {
    double score      = 0.0;                       // /v0/subjects/{id} 嵌入 rating.score (double)
    std::string scoreText;                         // /v0/subjects/{id}/rating score (string, e.g. "7.8")
    int    total      = 0;
    int    rank       = 0;
    int    count_1 = 0, count_2 = 0, count_3 = 0, count_4 = 0, count_5 = 0;
    int    count_6 = 0, count_7 = 0, count_8 = 0, count_9 = 0, count_10 = 0;
    std::vector<int32_t> bucketCount;              // /rating endpoint count[] array (10 buckets)
};

struct SubjectCollection {
    int wish    = 0;
    int collect = 0;
    int doing   = 0;
    int onHold  = 0;
    int dropped = 0;
};

struct Subject {
    int32_t     id          = 0;
    int32_t     type        = 2;       // 1=book 2=anime 3=music 4=game 6=real
    std::string typeName;
    std::string name;
    std::string nameCN;
    std::string summary;
    std::string date;                  // YYYY-MM-DD
    std::string platform;
    std::string url;
    int32_t     eps         = 0;       // total
    int32_t     totalEps    = 0;       // "X 话"
    int32_t     ratingCount = 0;
    SubjectImage    images;
    SubjectRating   rating;
    SubjectCollection collection;
    std::vector<std::string> tags;
    std::vector<std::string> metaTags;
    bool        nsfw        = false;
};

// ---- Episode ------------------------------------------------------------

struct Episode {
    int32_t     id         = 0;
    int32_t     type       = 0;        // 0=本篇 1=SP 2=OP 3=ED 4=PV 5=MAD ...
    double      sort       = 0;
    std::string name;
    std::string nameCN;
    int32_t     duration   = 0;        // seconds
    std::string airdate;
    std::string desc;
    int32_t     subjectID  = 0;        // joining back to Subject.id
    double      ep         = 0;        // 第几话 (0 for specials)
    int32_t     disc       = 0;        // disc number for BD/DVD
    std::string url;                  // canonical episode page on bangumi.tv
};

// ---- Person (staff / cast) ----------------------------------------------

struct Person {
    int32_t     id = 0;
    std::string name;
    std::string nameCN;
    int32_t type = 0;                 // 1=个人 2=公司 3=组合
    std::vector<std::string> career;
    std::string image;
    std::string url;
};

struct SubjectRelation {
    int32_t     id   = 0;
    std::string type;                 // "续集" / "前传" / "系列" ...
    std::string order;                // "0" / "1" ...
    Subject     subject;
};

struct SubjectCharacter {
    int32_t     id = 0;
    std::string name;
    std::string nameCN;
    std::string role;                 // "主角" / "配角" / "客串"
    int32_t     actorID = 0;
    std::string actor;
};

// ---- Subject comments (Bangumi /v0/subjects/{id}/comments) -------------

struct CommentUserBrief {
    int32_t     id = 0;
    std::string username;
    std::string nickname;
    std::string avatar;       // small / medium / large
};

struct Comment {
    int32_t id = 0;
    CommentUserBrief user;
    int     rate = 0;          // 0-10, 0 means no rating
    std::string content;      // raw text (may contain BBCode)
    std::string createdAt;     // ISO 8601
    int     replies = 0;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CommentUserBrief, id, username, nickname, avatar);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Comment, id, user, rate, content, createdAt, replies);


struct UserCollection {
    int32_t     subjectId    = 0;
    int32_t     type         = 3;      // 1=wish 2=doing 3=collect 4=on_hold 5=dropped
    std::string comment;
    int32_t     rating       = 0;      // 0-10
    bool        private_     = false;
    std::string updatedAt;
    int32_t     epStatus     = 0;      // watched episode count
    Subject     subject;
};

// ---- Calendar (每日放送) -----------------------------------------------

struct CalendarItem {
    int32_t     weekday = 0;           // 0=Sunday .. 6=Saturday
    Subject     subject;
};

// ---- Search result (v0 uses subject search) ----------------------------

struct SearchSubject {
    int32_t     id = 0;
    std::string name;
    std::string nameCN;
    int32_t     type = 0;
    std::string typeName;
    std::string date;
    SubjectImage images;
    int32_t     eps = 0;
    int32_t     totalEps = 0;
    std::string summary;
    std::vector<std::string> metaTags;
    double      score = 0.0;
    int         rank = 0;
};

struct SearchSubjectList {
    int total = 0;
    int limit = 0;
    int offset = 0;
    std::vector<SearchSubject> data;
};

// ---- Auth --------------------------------------------------------------

struct OAuthToken {
    std::string accessToken;
    std::string refreshToken;
    std::string tokenType = "Bearer";
    int64_t     expiresIn = 0;        // seconds
    std::string scope;
    int64_t     obtainedAt = 0;       // unix seconds
    std::string userId;
};

// ---- Response envelopes ------------------------------------------------
//
// Bangumi's public API returns either `{code, message, result}` or
// `{request, code, message, result}`. The HTTP wrapper already extracts
// `result` for us, so the JSON bindings below only need to handle the
// payload inside `result`.

inline void from_json(const nlohmann::json& j, SubjectImage& v) {
    readJsonField(j, "large", v.large);
    readJsonField(j, "common", v.common);
    readJsonField(j, "medium", v.medium);
    readJsonField(j, "small", v.small);
    readJsonField(j, "grid", v.grid);
}

inline void from_json(const nlohmann::json& j, SubjectRating& v) {
    if (j.contains("score") && j["score"].is_string())
        readJsonField(j, "score", v.scoreText);
    else
        readJsonField(j, "score", v.score);
    readJsonField(j, "total", v.total);
    readJsonField(j, "rank", v.rank);
    if (j.contains("count") && j["count"].is_object()) {
        int* counts[] = {&v.count_1, &v.count_2, &v.count_3, &v.count_4, &v.count_5,
                         &v.count_6, &v.count_7, &v.count_8, &v.count_9, &v.count_10};
        for (int i = 0; i < 10; ++i)
            readJsonField(j["count"], std::to_string(i + 1).c_str(), *counts[i]);
    } else if (j.contains("count") && j["count"].is_array()) {
        for (const auto& c : j["count"]) v.bucketCount.push_back(c.get<int32_t>());
    }
}

inline void from_json(const nlohmann::json& j, SubjectCollection& v) {
    readJsonField(j, "wish", v.wish);
    readJsonField(j, "collect", v.collect);
    readJsonField(j, "doing", v.doing);
    readJsonField(j, "on_hold", v.onHold);
    readJsonField(j, "dropped", v.dropped);
}

inline void from_json(const nlohmann::json& j, Subject& v) {
    readJsonField(j, "id", v.id);
    readJsonField(j, "type", v.type);
    readJsonField(j, "name", v.name);
    readJsonField(j, "name_cn", v.nameCN);
    readJsonField(j, "summary", v.summary);
    readJsonField(j, "date", v.date);
    readJsonField(j, "platform", v.platform);
    readJsonField(j, "url", v.url);
    readJsonField(j, "eps", v.eps);
    readJsonField(j, "total_episodes", v.totalEps);
    readJsonField(j, "images", v.images);
    readJsonField(j, "rating", v.rating);
    readJsonField(j, "collection", v.collection);
    readJsonField(j, "meta_tags", v.metaTags);
    readJsonField(j, "nsfw", v.nsfw);
    v.ratingCount = v.rating.total;
    if (j.contains("tags") && j["tags"].is_array()) {
        v.tags.clear();
        for (const auto& tag : j["tags"])
            v.tags.push_back(tag.at("name").get<std::string>());
    }
}

inline void from_json(const nlohmann::json& j, Episode& v) {
    readJsonField(j, "id", v.id);
    readJsonField(j, "type", v.type);
    readJsonField(j, "sort", v.sort);
    readJsonField(j, "name", v.name);
    readJsonField(j, "name_cn", v.nameCN);
    readJsonField(j, "duration_seconds", v.duration);
    readJsonField(j, "airdate", v.airdate);
    readJsonField(j, "desc", v.desc);
    readJsonField(j, "subject_id", v.subjectID);
    readJsonField(j, "ep", v.ep);
    readJsonField(j, "disc", v.disc);
}

inline void from_json(const nlohmann::json& j, Person& v) {
    readJsonField(j, "id", v.id);
    readJsonField(j, "name", v.name);
    readJsonField(j, "type", v.type);
    readJsonField(j, "career", v.career);
    if (j.contains("images") && j["images"].is_object())
        readJsonField(j["images"], "medium", v.image);
}

inline void from_json(const nlohmann::json& j, SubjectRelation& v) {
    readJsonField(j, "id", v.id);
    readJsonField(j, "relation", v.type);
    v.subject = j.get<Subject>();
}

inline void from_json(const nlohmann::json& j, SubjectCharacter& v) {
    readJsonField(j, "id", v.id);
    readJsonField(j, "name", v.name);
    readJsonField(j, "relation", v.role);
    if (j.contains("actors") && j["actors"].is_array() && !j["actors"].empty()) {
        readJsonField(j["actors"][0], "id", v.actorID);
        readJsonField(j["actors"][0], "name", v.actor);
    }
}

inline void from_json(const nlohmann::json& j, UserCollection& v) {
    readJsonField(j, "subject_id", v.subjectId);
    readJsonField(j, "type", v.type);
    readJsonField(j, "comment", v.comment);
    readJsonField(j, "rate", v.rating);
    readJsonField(j, "private", v.private_);
    readJsonField(j, "updated_at", v.updatedAt);
    readJsonField(j, "ep_status", v.epStatus);
    readJsonField(j, "subject", v.subject);
}

inline void from_json(const nlohmann::json& j, SearchSubject& v) {
    auto subject = j.get<Subject>();
    v.id = subject.id;
    v.type = subject.type;
    v.name = std::move(subject.name);
    v.nameCN = std::move(subject.nameCN);
    v.date = std::move(subject.date);
    v.images = std::move(subject.images);
    v.eps = subject.eps;
    v.totalEps = subject.totalEps;
    v.summary = std::move(subject.summary);
    v.metaTags = std::move(subject.metaTags);
    v.score = subject.rating.score;
    v.rank = subject.rating.rank;
}

inline void from_json(const nlohmann::json& j, SearchSubjectList& v) {
    readJsonField(j, "total", v.total);
    readJsonField(j, "limit", v.limit);
    readJsonField(j, "offset", v.offset);
    readJsonField(j, "data", v.data);
}

}  // namespace aniswitch
