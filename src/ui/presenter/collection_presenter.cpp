// SPDX-License-Identifier: AGPL-3.0

#include "ui/presenter/collection_presenter.hpp"
#include "ui/presenter/presenter.hpp"
#include "net/bgm_client.hpp"
#include "utils/config_helper.hpp"
#include <borealis/core/logger.hpp>
#include <borealis/core/thread.hpp>

namespace aniswitch {

// ----------------------------------------------------------------
// v18.9: single-subject collection editor.
// ----------------------------------------------------------------

void SingleCollectionEditor::setCollectionType(int32_t subjectId, int32_t type) {
    auto& cfg = ProgramConfig::instance();
    if (subjectId <= 0) {
        onUpdated.fire(-1);
        return;
    }
    if (type < 0 || type > 5) {
        brls::Logger::warning("SingleCollectionEditor: bad type {}", type);
        onUpdated.fire(-1);
        return;
    }
    // Bangumi's /v0/users/-/collections/{id} uses `username="-"`
    // as a placeholder for the current user.  v18.9 passes the
    // literal "-" so cpr sends it as a path segment; the API
    // server resolves it back to whoever owns the bearer token.
    // type=0 is "remove from collection" — Bangumi interprets
    // 0 / missing type as a delete, but to be explicit we
    // thread it through anyway.
    BangumiClient::updateUserCollection(
        "-", subjectId, type,
        /*rate=*/0, /*comment=*/"", /*epStatus=*/0, /*private=*/false,
        uiCallback([this, type](void) { onUpdated.fire(type); }),
        [this](const std::string& msg, int) {
            brls::Logger::warning("SingleCollectionEditor: {}", msg);
            onUpdated.fire(-1);
        });
}

// ----------------------------------------------------------------
// v17.0: list presenter (unchanged).
// ----------------------------------------------------------------

void CollectionPresenter::refreshFromRemote() {
    auto& cfg = ProgramConfig::instance();
    if (!cfg.hasLoginInfo()) {
        refreshFromLocal();
        return;
    }
    // v17.0 used a fixed "all types" query; type_ defaults to
    // 0 which is "all" in the Bangumi client.
    BangumiClient::getUserCollection(
        cfg.getUserID(), type_, 50,
        [this](std::vector<UserCollection> remote) {
            std::vector<SQLiteStore::CollectionEntry> out;
            out.reserve(remote.size());
            for (auto& r : remote) {
                SQLiteStore::CollectionEntry e{};
                e.subjectId  = r.subjectId;
                e.type       = r.type;
                e.rating     = r.rating;
                e.comment    = r.comment;
                out.push_back(std::move(e));
            }
            onCollection.fire(std::move(out));
        },
        [this](const std::string& msg, int) {
            onError.fire(msg);
            refreshFromLocal();
        });
}

void CollectionPresenter::refreshFromLocal() {
    auto rows = SQLiteStore::instance().getCollection(type_, 50);
    std::vector<SQLiteStore::CollectionEntry> out;
    out.reserve(rows.size());
    for (auto& r : rows) {
        SQLiteStore::CollectionEntry e{};
        e.subjectId = r.subjectId;
        e.type      = r.type;
        e.rating    = r.rating;
        e.comment   = r.comment;
        out.push_back(std::move(e));
    }
    onCollection.fire(std::move(out));
}

}  // namespace aniswitch
