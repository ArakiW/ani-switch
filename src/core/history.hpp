// SPDX-License-Identifier: AGPL-3.0
//
// Watch history manager. Persists every played episode to SQLite, and
// surfaces them as the "history" tab. Also keeps a per-episode progress
// record so the player can resume to the right offset.

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "utils/sqlite_store.hpp"

namespace aniswitch {

class HistoryManager {
public:
    static HistoryManager& instance();

    // Record a watch event (called when the user leaves a player or
    // hits a periodic checkpoint).
    void record(int32_t subjectId,
                int32_t episodeId,
                const std::string& subjectName,
                const std::string& episodeName,
                int64_t positionMs,
                int64_t durationMs);

    // Fetch the most recent N entries (default 50).
    std::vector<SQLiteStore::HistoryEntry> recent(int limit = 50);

    // Delete a history entry (the user pressed "remove from history").
    bool remove(int32_t episodeId);

    // Fetch progress for one episode.
    std::optional<SQLiteStore::ProgressEntry> progress(int32_t episodeId);

    // Save progress (the player writes this every 10s).
    bool saveProgress(int32_t episodeId, int32_t subjectId,
                      int64_t positionMs, int64_t durationMs);
};

}  // namespace aniswitch
