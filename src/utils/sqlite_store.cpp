// SPDX-License-Identifier: AGPL-3.0
//
// SQLiteStore — JSON-file-backed persistent store.  The class name and
// public API are unchanged from the original SQLite implementation; only
// the backing storage swapped because the Switch devoptab returns EIO on
// `sdmc:` paths during `sqlite3_open_v2` regardless of journal / lock /
// stale-file workarounds.  The on-disk file is `<config_dir>/<basename>`
// — callers pass `ani-switch.store.json` from config_helper; the .db
// extension is no longer used.

#include "utils/sqlite_store.hpp"
#include <nlohmann/json.hpp>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <fstream>
#include <memory>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _WIN32
#include <direct.h>
#endif

#if defined(__SWITCH__) && defined(ANISWITCH_SWITCH_DEBUG)
extern "C" void aniswitchStartupLog(const char*);
#endif

namespace aniswitch {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SQLiteStore::CollectionEntry, subjectId, type, comment, rating, private_, updatedAt, name, nameCN, cover, eps, totalEps)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SQLiteStore::HistoryEntry, subjectId, episodeId, subjectName, episodeName, positionMs, durationMs, watchedAt)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SQLiteStore::ProgressEntry, episodeId, subjectId, positionMs, durationMs, updatedAt)

namespace {
void storeLog(const char* message) {
#if defined(__SWITCH__) && defined(ANISWITCH_SWITCH_DEBUG)
    aniswitchStartupLog(message);
#else
    std::fprintf(stderr, "%s\n", message);
#endif
}

void storeLogErr(const char* stage, const std::string& path, int err) {
    char msg[512];
    std::snprintf(msg, sizeof(msg), "store: %s path=%s errno=%d", stage, path.c_str(), err);
    storeLog(msg);
}

struct Document {
    int version = 1;
    std::vector<SQLiteStore::CollectionEntry> collection;
    std::vector<SQLiteStore::HistoryEntry>   history;
    std::vector<SQLiteStore::ProgressEntry>  progress;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Document, version, collection, history, progress)

bool writeAtomic(const std::string& path, const std::string& content) {
    // Direct overwrite: the store is small (<10 KB) and single-process, so
    // crash-safety via tmp+rename buys little.  We use direct truncate on
    // Windows (MinGW) and Switch (libnx devoptab) because both surface
    // POSIX `rename(2)` as a non-replacing move (EEXIST when the
    // destination already exists), unlike Linux where rename atomically
    // replaces the destination.
#if defined(_WIN32) || defined(__SWITCH__)
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) { storeLogErr("open for write failed", path, errno); return false; }
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!f) { storeLogErr("write failed", path, errno); return false; }
    f.flush();
    f.close();
    if (!f) return false;
    return true;
#else
    const std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) { storeLogErr("open tmp for write failed", tmp, errno); return false; }
        f.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!f) { storeLogErr("tmp write failed", tmp, errno); f.close(); ::unlink(tmp.c_str()); return false; }
        f.flush();
        f.close();
    }
    if (::rename(tmp.c_str(), path.c_str()) != 0) {
        storeLogErr("rename failed", tmp, errno);
        ::unlink(tmp.c_str());
        return false;
    }
    return true;
#endif
}
}  // namespace

struct SQLiteStore::Impl {
    std::string            path;
    std::vector<CollectionEntry> collection;
    std::vector<HistoryEntry>    history;
    std::vector<ProgressEntry>   progress;
    bool                   loaded = false;

    bool load() {
        loaded = true;
        std::ifstream f(path, std::ios::binary);
        if (!f) return true;                       // missing -> empty
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (content.empty()) return true;          // empty file -> empty
        try {
            auto doc = nlohmann::json::parse(content).get<Document>();
            collection = std::move(doc.collection);
            history    = std::move(doc.history);
            progress   = std::move(doc.progress);
        } catch (const std::exception& e) {
            char msg[256];
            std::snprintf(msg, sizeof(msg), "store: parse failed: %s", e.what());
            storeLog(msg);
            return false;
        }
        return true;
    }

    bool flush() const {
        Document doc;
        doc.collection = collection;
        doc.history    = history;
        doc.progress   = progress;
        return writeAtomic(path, nlohmann::json(doc).dump());
    }

    void upsertCollectionLocked(const CollectionEntry& e) {
        auto it = std::find_if(collection.begin(), collection.end(),
            [&](const CollectionEntry& x) { return x.subjectId == e.subjectId; });
        CollectionEntry merged = e;
        if (!merged.updatedAt) merged.updatedAt = static_cast<int64_t>(std::time(nullptr));
        if (it == collection.end()) collection.push_back(std::move(merged));
        else                         *it = std::move(merged);
    }

    void upsertHistoryLocked(const HistoryEntry& e) {
        auto it = std::find_if(history.begin(), history.end(),
            [&](const HistoryEntry& x) { return x.episodeId == e.episodeId; });
        HistoryEntry merged = e;
        if (!merged.watchedAt) merged.watchedAt = static_cast<int64_t>(std::time(nullptr));
        if (it == history.end()) history.push_back(std::move(merged));
        else                     *it = std::move(merged);
    }

    void upsertProgressLocked(const ProgressEntry& e) {
        auto it = std::find_if(progress.begin(), progress.end(),
            [&](const ProgressEntry& x) { return x.episodeId == e.episodeId; });
        ProgressEntry merged = e;
        if (!merged.updatedAt) merged.updatedAt = static_cast<int64_t>(std::time(nullptr));
        if (it == progress.end()) progress.push_back(std::move(merged));
        else                      *it = std::move(merged);
    }
};

SQLiteStore& SQLiteStore::instance() { static SQLiteStore store; return store; }
SQLiteStore::SQLiteStore() = default;
SQLiteStore::~SQLiteStore() { close(); }
int64_t SQLiteStore::nowUnix() const { return static_cast<int64_t>(std::time(nullptr)); }

bool SQLiteStore::open(const std::string& path) {
    std::lock_guard<std::mutex> lock(mu_);
    if (impl_) return true;
    auto impl = std::make_unique<Impl>();
    impl->path = path;
    {
        auto slash = path.find_last_of("/\\");
        if (slash != std::string::npos) {
            std::string dir = path.substr(0, slash);
            if (!dir.empty()) {
#ifdef _WIN32
                int rc = ::_mkdir(dir.c_str());
#else
                int rc = ::mkdir(dir.c_str(), 0777);
#endif
                if (rc != 0 && errno != EEXIST)
                    storeLogErr("mkdir failed", dir, errno);
            }
        }
    }
    char msg[512];
    std::snprintf(msg, sizeof(msg), "store: open %s", path.c_str());
    storeLog(msg);
    if (!impl->load()) { return false; }
    if (!impl->flush()) { return false; }   // ensure file exists for next reload
    storeLog("store: persistent storage ready");
    impl_ = std::move(impl);
    return true;
}

void SQLiteStore::close() {
    std::unique_ptr<Impl> taken;
    {
        std::lock_guard<std::mutex> lock(mu_);
        taken = std::move(impl_);
    }
    if (taken) taken->flush();
}

std::vector<SQLiteStore::CollectionEntry> SQLiteStore::getCollection(int32_t type, int limit) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!impl_) return {};
    std::vector<CollectionEntry> out;
    out.reserve(impl_->collection.size());
    for (const auto& e : impl_->collection) {
        if (type < 0 || e.type == type) out.push_back(e);
    }
    std::sort(out.begin(), out.end(), [](const CollectionEntry& a, const CollectionEntry& b) {
        return a.updatedAt > b.updatedAt;
    });
    if (limit >= 0 && static_cast<int>(out.size()) > limit) out.resize(static_cast<size_t>(limit));
    return out;
}

bool SQLiteStore::upsertCollection(const CollectionEntry& entry) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!impl_) return false;
    impl_->upsertCollectionLocked(entry);
    return impl_->flush();
}

bool SQLiteStore::deleteCollection(int32_t id) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!impl_) return false;
    auto before = impl_->collection.size();
    impl_->collection.erase(
        std::remove_if(impl_->collection.begin(), impl_->collection.end(),
            [id](const CollectionEntry& x) { return x.subjectId == id; }),
        impl_->collection.end());
    if (impl_->collection.size() == before) return false;
    return impl_->flush();
}

std::vector<SQLiteStore::HistoryEntry> SQLiteStore::getHistory(int limit) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!impl_) return {};
    std::vector<HistoryEntry> out = impl_->history;
    std::sort(out.begin(), out.end(), [](const HistoryEntry& a, const HistoryEntry& b) {
        return a.watchedAt > b.watchedAt;
    });
    if (limit >= 0 && static_cast<int>(out.size()) > limit) out.resize(static_cast<size_t>(limit));
    return out;
}

bool SQLiteStore::upsertHistory(const HistoryEntry& entry) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!impl_) return false;
    impl_->upsertHistoryLocked(entry);
    return impl_->flush();
}

bool SQLiteStore::deleteHistory(int32_t id) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!impl_) return false;
    auto before = impl_->history.size();
    impl_->history.erase(
        std::remove_if(impl_->history.begin(), impl_->history.end(),
            [id](const HistoryEntry& x) { return x.episodeId == id; }),
        impl_->history.end());
    if (impl_->history.size() == before) return false;
    return impl_->flush();
}

size_t SQLiteStore::clearAllHistory() {
    std::lock_guard<std::mutex> lock(mu_);
    if (!impl_) return 0;
    auto removed = impl_->history.size();
    if (removed == 0) return 0;
    impl_->history.clear();
    impl_->flush();
    return removed;
}

std::optional<SQLiteStore::ProgressEntry> SQLiteStore::getProgress(int32_t id) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!impl_) return std::nullopt;
    for (const auto& p : impl_->progress) if (p.episodeId == id) return p;
    return std::nullopt;
}

bool SQLiteStore::upsertProgress(const ProgressEntry& entry) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!impl_) return false;
    impl_->upsertProgressLocked(entry);
    return impl_->flush();
}

std::optional<SQLiteStore::HistoryEntry> SQLiteStore::getLastEpisodeForSubject(int32_t id) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!impl_) return std::nullopt;
    std::optional<HistoryEntry> best;
    for (const auto& h : impl_->history) {
        if (h.subjectId != id) continue;
        if (!best || h.watchedAt > best->watchedAt) best = h;
    }
    return best;
}
}  // namespace aniswitch
