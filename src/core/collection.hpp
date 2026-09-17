// SPDX-License-Identifier: AGPL-3.0
//
// User collection manager. Wraps SQLiteStore for local persistence and
// BangumiClient for syncing the user's collection state to/from
// bangumi.tv.
//
// Bangumi collection type IDs:
//   1 = 想看  (wish)
//   2 = 在看  (doing)
//   3 = 看过  (collect)
//   4 = 搁置  (on hold)
//   5 = 抛弃  (dropped)

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "net/bgm_types.hpp"
#include "utils/sqlite_store.hpp"

namespace aniswitch {

class CollectionManager {
public:
    static CollectionManager& instance();

    // Pull remote collection, replace local cache, return ordered list.
    using ListCb = std::function<void(std::vector<SQLiteStore::CollectionEntry>)>;
    void refreshFromRemote(int32_t type, ListCb callback,
                           std::function<void(const std::string&, int)> error);

    // Update a single entry on both server and local.
    using UpdateCb = std::function<void()>;
    void update(int32_t subjectId, int32_t type, int32_t rating,
                const std::string& comment, bool private_,
                UpdateCb callback,
                std::function<void(const std::string&, int)> error);

    // Local read of the cached collection.
    std::vector<SQLiteStore::CollectionEntry> getLocal(int32_t type = -1, int limit = 100);

    // Get a single entry by subject id.
    std::optional<SQLiteStore::CollectionEntry> getLocalOne(int32_t subjectId);
};

}  // namespace aniswitch
