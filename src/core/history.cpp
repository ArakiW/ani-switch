// SPDX-License-Identifier: AGPL-3.0

#include "core/history.hpp"

namespace aniswitch {

HistoryManager& HistoryManager::instance() {
    static HistoryManager s;
    return s;
}

void HistoryManager::record(int32_t subjectId, int32_t episodeId,
                            const std::string& subjectName,
                            const std::string& episodeName,
                            int64_t positionMs, int64_t durationMs) {
    SQLiteStore::HistoryEntry e;
    e.subjectId   = subjectId;
    e.episodeId   = episodeId;
    e.subjectName = subjectName;
    e.episodeName = episodeName;
    e.positionMs  = positionMs;
    e.durationMs  = durationMs;
    e.watchedAt   = static_cast<int64_t>(time(nullptr));
    SQLiteStore::instance().upsertHistory(e);
}

std::vector<SQLiteStore::HistoryEntry> HistoryManager::recent(int limit) {
    return SQLiteStore::instance().getHistory(limit);
}

bool HistoryManager::remove(int32_t episodeId) {
    return SQLiteStore::instance().deleteHistory(episodeId);
}

std::optional<SQLiteStore::ProgressEntry> HistoryManager::progress(int32_t episodeId) {
    return SQLiteStore::instance().getProgress(episodeId);
}

bool HistoryManager::saveProgress(int32_t episodeId, int32_t subjectId,
                                  int64_t positionMs, int64_t durationMs) {
    SQLiteStore::ProgressEntry p;
    p.episodeId  = episodeId;
    p.subjectId  = subjectId;
    p.positionMs = positionMs;
    p.durationMs = durationMs;
    p.updatedAt  = static_cast<int64_t>(time(nullptr));
    return SQLiteStore::instance().upsertProgress(p);
}

}  // namespace aniswitch
