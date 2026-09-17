// SPDX-License-Identifier: AGPL-3.0

#include "core/collection.hpp"
#include "net/bgm_client.hpp"
#include "utils/config_helper.hpp"
#include <borealis/core/logger.hpp>

namespace aniswitch {

CollectionManager& CollectionManager::instance() {
    static CollectionManager s;
    return s;
}

void CollectionManager::refreshFromRemote(int32_t type, ListCb callback,
                                          std::function<void(const std::string&, int)> error) {
    const auto& userId = ProgramConfig::instance().getUserID();
    if (userId.empty()) {
        if (error) error("Not logged in", 401);
        return;
    }
    BangumiClient::getUserCollection(userId, type, 50,
        [callback, error](std::vector<UserCollection> remote) {
            std::vector<SQLiteStore::CollectionEntry> out;
            out.reserve(remote.size());
            for (const auto& r : remote) {
                SQLiteStore::CollectionEntry e;
                e.subjectId = r.subjectId;
                e.type      = r.type;
                e.comment   = r.comment;
                e.rating    = r.rating;
                e.private_  = r.private_;
                e.updatedAt = static_cast<int64_t>(time(nullptr));
                if (r.subject.id != 0) {
                    e.name      = r.subject.name;
                    e.nameCN    = r.subject.nameCN;
                    e.cover     = r.subject.images.common;
                    e.eps       = r.subject.eps;
                    e.totalEps  = r.subject.totalEps;
                }
                SQLiteStore::instance().upsertCollection(e);
                out.push_back(std::move(e));
            }
            if (callback) callback(std::move(out));
        }, error);
}

void CollectionManager::update(int32_t subjectId, int32_t type, int32_t rating,
                               const std::string& comment, bool private_,
                               UpdateCb callback,
                               std::function<void(const std::string&, int)> error) {
    const auto& userId = ProgramConfig::instance().getUserID();
    if (userId.empty()) {
        if (error) error("Not logged in", 401);
        return;
    }
    BangumiClient::updateUserCollection(userId, subjectId, type, rating, comment,
                                        0, private_,
        [subjectId, type, rating, comment, private_, callback](void) {
            SQLiteStore::CollectionEntry e;
            e.subjectId = subjectId;
            e.type      = type;
            e.rating    = rating;
            e.comment   = comment;
            e.private_  = private_;
            e.updatedAt = static_cast<int64_t>(time(nullptr));
            SQLiteStore::instance().upsertCollection(e);
            if (callback) callback();
        },
        error);
}

std::vector<SQLiteStore::CollectionEntry> CollectionManager::getLocal(int32_t type, int limit) {
    return SQLiteStore::instance().getCollection(type, limit);
}

std::optional<SQLiteStore::CollectionEntry> CollectionManager::getLocalOne(int32_t subjectId) {
    auto all = SQLiteStore::instance().getCollection(-1, 1000);
    for (auto& e : all) {
        if (e.subjectId == subjectId) return e;
    }
    return std::nullopt;
}

}  // namespace aniswitch
