// SPDX-License-Identifier: AGPL-3.0
// Persistent collection, history and progress.
//
// Implementation note: the public API and class name are preserved from
// the original SQLite-backed version.  v3/v4/v5 of the NRO all failed
// with EIO on `sdmc:` paths inside `sqlite3_open_v2` regardless of
// journal/lock/stale-file workarounds, so the backing store was swapped
// to a single JSON document.  Watch-history / collection / progress is
// tiny (<10 KB typical), so per-mutation rewrite + atomic rename is
// cheap and avoids the devoptab's open(2) quirks entirely.

#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace aniswitch {

class SQLiteStore {
public:
    static SQLiteStore& instance();

    // Open (or create) the persistent store at <path>.
    bool open(const std::string& path);
    void close();

    // ---- Bangumi user collection ---------------------------------------
    struct CollectionEntry {
        int32_t subjectId    = 0;
        int32_t type         = 0;   // 1=wish 2=doing 3=collect 4=on_hold 5=dropped
        std::string comment;
        int32_t rating       = 0;   // 0-10
        bool    private_     = false;
        int64_t updatedAt    = 0;
        // Joins populated on read:
        std::string name;
        std::string nameCN;
        std::string cover;
        int32_t     eps        = 0;
        int32_t     totalEps   = 0;
    };

    std::vector<CollectionEntry> getCollection(int32_t type = -1, int limit = 100);
    bool upsertCollection(const CollectionEntry& e);
    bool deleteCollection(int32_t subjectId);

    // ---- Watch history --------------------------------------------------
    struct HistoryEntry {
        int32_t     subjectId   = 0;
        int32_t     episodeId   = 0;
        std::string subjectName;
        std::string episodeName;
        int64_t     positionMs  = 0;
        int64_t     durationMs  = 0;
        int64_t     watchedAt   = 0;
    };

    std::vector<HistoryEntry> getHistory(int limit = 50);
    bool upsertHistory(const HistoryEntry& e);
    bool deleteHistory(int32_t episodeId);
    // v17.5: nuke every history row.  Returns the number of
    // rows removed.
    size_t clearAllHistory();

    // ---- Episode progress (per episode) --------------------------------
    struct ProgressEntry {
        int32_t episodeId    = 0;
        int32_t subjectId    = 0;
        int64_t positionMs   = 0;
        int64_t durationMs   = 0;
        int64_t updatedAt    = 0;
    };

    std::optional<ProgressEntry> getProgress(int32_t episodeId);
    bool upsertProgress(const ProgressEntry& e);

    // ---- Last watched episode (for "continue" on subject screen) --------
    std::optional<HistoryEntry> getLastEpisodeForSubject(int32_t subjectId);

private:
    SQLiteStore();
    ~SQLiteStore();
    SQLiteStore(const SQLiteStore&) = delete;
    SQLiteStore& operator=(const SQLiteStore&) = delete;

    int64_t nowUnix() const;

    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::mutex mu_;
};

}  // namespace aniswitch
