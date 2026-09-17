// SPDX-License-Identifier: AGPL-3.0
//
// Smoke test for SQLiteStore.  Backed by a JSON file in the temp
// directory; the public API is unchanged from the SQLite version.

#include "utils/sqlite_store.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <chrono>

#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL: %s @ %d\n", #cond, __LINE__); \
        std::exit(1); \
    } \
} while (0)

int main() {
    using namespace aniswitch;

    auto path = (std::filesystem::temp_directory_path() /
        ("aniswitch-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json")).string();

    EXPECT(SQLiteStore::instance().open(path));
    SQLiteStore::instance().close();

    EXPECT(SQLiteStore::instance().open(path));
    {
        SQLiteStore::CollectionEntry e;
        e.subjectId = 1;
        e.type      = 3;       // collect
        e.rating    = 9;
        e.comment   = "good";
        e.name      = "Cowboy Bebop";
        e.nameCN    = "星际牛仔";
        e.cover     = "https://example.com/cover.jpg";
        e.eps       = 26;
        e.totalEps  = 26;
        EXPECT(SQLiteStore::instance().upsertCollection(e));
    }
    {
        SQLiteStore::CollectionEntry e2;
        e2.subjectId = 2;
        e2.type      = 2;       // doing
        e2.name      = "Planetes";
        e2.nameCN    = "星空之旅";
        EXPECT(SQLiteStore::instance().upsertCollection(e2));
    }
    auto coll = SQLiteStore::instance().getCollection(-1, 100);
    EXPECT(coll.size() == 2);

    // Upsert (replace) the first entry
    {
        SQLiteStore::CollectionEntry e;
        e.subjectId = 1;
        e.type      = 5;       // dropped
        e.rating    = 4;
        EXPECT(SQLiteStore::instance().upsertCollection(e));
    }
    coll = SQLiteStore::instance().getCollection(5, 100);
    EXPECT(coll.size() == 1);
    EXPECT(coll[0].subjectId == 1);
    EXPECT(coll[0].type      == 5);

    // History + progress
    {
        SQLiteStore::HistoryEntry h;
        h.episodeId   = 100;
        h.subjectId   = 1;
        h.subjectName = "Cowboy Bebop";
        h.episodeName = "EP1";
        h.positionMs  = 60000;
        h.durationMs  = 1500000;
        EXPECT(SQLiteStore::instance().upsertHistory(h));
    }
    {
        SQLiteStore::ProgressEntry p;
        p.episodeId  = 100;
        p.subjectId  = 1;
        p.positionMs = 12345;
        p.durationMs = 1500000;
        EXPECT(SQLiteStore::instance().upsertProgress(p));
    }
    auto hist = SQLiteStore::instance().getHistory(10);
    EXPECT(hist.size() == 1);
    EXPECT(hist[0].episodeId == 100);

    auto prog = SQLiteStore::instance().getProgress(100);
    EXPECT(prog.has_value());
    EXPECT(prog->positionMs == 12345);

    // "Last episode for subject" lookup
    auto last = SQLiteStore::instance().getLastEpisodeForSubject(1);
    EXPECT(last.has_value());
    EXPECT(last->episodeId == 100);

    // Delete
    EXPECT(SQLiteStore::instance().deleteCollection(1));
    coll = SQLiteStore::instance().getCollection(-1, 100);
    EXPECT(coll.size() == 1);
    EXPECT(coll[0].subjectId == 2);

    SQLiteStore::instance().close();
    EXPECT(SQLiteStore::instance().open(path));
    EXPECT(SQLiteStore::instance().getProgress(100)->positionMs == 12345);
    EXPECT(SQLiteStore::instance().getHistory(10).size() == 1);
    EXPECT(SQLiteStore::instance().getCollection(-1, 0).empty());
    EXPECT(SQLiteStore::instance().deleteHistory(100));
    EXPECT(SQLiteStore::instance().getHistory(10).empty());
    SQLiteStore::instance().close();
    std::filesystem::remove(path);
    std::puts("test_sqlite_store: OK");
    return 0;
}
