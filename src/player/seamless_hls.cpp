// SPDX-License-Identifier: AGPL-3.0
//
// File-backed seamless stream for mpv stream_cb (wiliwili-style single
// demuxer timeline). HLS segments download via our HTTP stack into one
// growing file; mpv reads through `ani://` and blocks when it catches the
// writer — no playlist boundaries, no mid-episode jumps.

#include "player/seamless_hls.hpp"

#include "net/http.hpp"

#include <borealis/core/logger.hpp>
#include <cpr/cpr.h>
#include <mpv/client.h>
#include <mpv/stream_cb.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>

#if defined(__SWITCH__)
extern "C" void aniswitchStartupLog(const char*);
#define SHLOG(msg) aniswitchStartupLog(msg)
#else
#define SHLOG(msg) do { brls::Logger::info("{}", msg); } while (0)
#endif

namespace aniswitch {
namespace {

struct Session {
    std::string id;
    std::string path;
    std::string referer;
    std::vector<std::string> segs;
    std::mutex mu;
    std::condition_variable cv;
    std::thread worker;
    std::atomic<bool> cancelled{false};
    std::atomic<bool> downloadDone{false};
    std::atomic<size_t> bytes{0};
    std::atomic<size_t> segsDone{0};
    int totalDurationSec = 0;
    int totalSegs = 0;
    FILE* writer = nullptr;
};

struct Reader {
    std::shared_ptr<Session> s;
    FILE* fp = nullptr;
    int64_t pos = 0;
};

std::mutex g_mapMu;
std::map<std::string, std::shared_ptr<Session>> g_sessions;
std::atomic<uint64_t> g_nextId{1};

bool netFetchSeg(const std::string& url, const std::string& referer,
                 std::string& out, long* status) {
    cpr::Session s;
    HTTP::prepareFetchSession(s, url);
    s.SetTimeout(cpr::Timeout{60000});
    s.SetConnectTimeout(cpr::ConnectTimeout{15000});
    cpr::Header h{
        {"User-Agent",
         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"},
        {"Accept", "*/*"},
    };
    if (!referer.empty()) h["Referer"] = referer;
    s.SetHeader(h);
    auto r = s.Get();
    if (status) *status = r.status_code;
    if (r.error || r.status_code != 200) return false;
    out = std::move(r.text);
    return !out.empty();
}

void downloadWorker(std::shared_ptr<Session> s) {
    char msg[176];
    snprintf(msg, sizeof(msg), "SHLS: download start id=%s segs=%zu",
             s->id.c_str(), s->segs.size());
    SHLOG(msg);
    for (size_t i = 0; i < s->segs.size(); ++i) {
        if (s->cancelled.load()) break;
        std::string chunk;
        long st = 0;
        if (!netFetchSeg(s->segs[i], s->referer, chunk, &st) || chunk.empty()) {
            snprintf(msg, sizeof(msg), "SHLS: seg %zu fail st=%ld", i, st);
            SHLOG(msg);
            continue;
        }
        {
            std::lock_guard<std::mutex> lk(s->mu);
            if (s->writer) {
                fwrite(chunk.data(), 1, chunk.size(), s->writer);
                fflush(s->writer);
                s->bytes += chunk.size();
                s->segsDone += 1;
            }
        }
        s->cv.notify_all();
        if ((i + 1) % 10 == 0) {
            snprintf(msg, sizeof(msg), "SHLS: seg %zu/%zu bytes=%zu", i + 1,
                     s->segs.size(), s->bytes.load());
            SHLOG(msg);
        }
    }
    s->downloadDone = true;
    s->cv.notify_all();
    snprintf(msg, sizeof(msg),
             "SHLS: download done id=%s bytes=%zu segs=%zu", s->id.c_str(),
             s->bytes.load(), s->segsDone.load());
    SHLOG(msg);
}

std::shared_ptr<Session> findSession(const std::string& id) {
    std::lock_guard<std::mutex> lk(g_mapMu);
    auto it = g_sessions.find(id);
    return it == g_sessions.end() ? nullptr : it->second;
}

std::string uriToId(const char* uri) {
    std::string u = uri ? uri : "";
    const std::string p = "ani://";
    if (u.rfind(p, 0) != 0) return {};
    return u.substr(p.size());
}

int64_t rRead(void* cookie, char* buf, uint64_t nbytes) {
    auto* r = static_cast<Reader*>(cookie);
    if (!r || !r->s || !buf || !r->fp) return -1;
    auto& s = *r->s;
    if (s.cancelled.load()) return -1;

    std::unique_lock<std::mutex> lk(s.mu);
    // Block until writer advances past pos, or download finished.
    for (;;) {
        s.cv.wait_for(lk, std::chrono::milliseconds(400), [&] {
            return s.bytes.load() > static_cast<size_t>(r->pos) ||
                   s.downloadDone.load() || s.cancelled.load();
        });
        if (s.cancelled.load()) return -1;
        const size_t have = s.bytes.load();
        if (static_cast<size_t>(r->pos) < have) break;
        if (s.downloadDone.load()) return 0;  // true EOF
        // Caught up — keep waiting (never fake EOF mid-download).
    }
    const size_t have = s.bytes.load();
    const size_t avail = have - static_cast<size_t>(r->pos);
    const size_t want = avail < nbytes ? avail : static_cast<size_t>(nbytes);
    lk.unlock();

    for (int attempt = 0; attempt < 30; ++attempt) {
        if (s.cancelled.load()) return -1;
        if (fseek(r->fp, static_cast<long>(r->pos), SEEK_SET) == 0) {
            const size_t n = fread(buf, 1, want, r->fp);
            if (n > 0) {
                r->pos += static_cast<int64_t>(n);
                return static_cast<int64_t>(n);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        if (s.bytes.load() <= static_cast<size_t>(r->pos) &&
            s.downloadDone.load())
            return 0;
    }
    return rRead(cookie, buf, nbytes);
}

int64_t rSeek(void* cookie, int64_t offset) {
    auto* r = static_cast<Reader*>(cookie);
    if (!r || !r->s) return MPV_ERROR_GENERIC;
    if (offset < 0) return MPV_ERROR_UNSUPPORTED;
    auto& s = *r->s;
    if (offset == 0) {
        r->pos = 0;
        return 0;
    }
    // Forward seek beyond downloaded bytes: wait for writer (progress bar
    // uses full real_duration; user can drag ahead — buffer until then).
    std::unique_lock<std::mutex> lk(s.mu);
    const int64_t deadlineMs = 60000;
    int64_t waited = 0;
    while (static_cast<size_t>(offset) > s.bytes.load() &&
           !s.downloadDone.load() && !s.cancelled.load() && waited < deadlineMs) {
        s.cv.wait_for(lk, std::chrono::milliseconds(200));
        waited += 200;
    }
    if (s.cancelled.load()) return MPV_ERROR_GENERIC;
    const size_t have = s.bytes.load();
    if (static_cast<size_t>(offset) > have && !s.downloadDone.load())
        return MPV_ERROR_UNSUPPORTED;
    if (static_cast<size_t>(offset) > have && s.downloadDone.load()) {
        // Clamp to end of file if user seeks past what we managed to download.
        r->pos = static_cast<int64_t>(have);
        return r->pos;
    }
    r->pos = offset;
    return offset;
}

int64_t rSize(void* cookie) {
    auto* r = static_cast<Reader*>(cookie);
    if (!r || !r->s) return MPV_ERROR_UNSUPPORTED;
    auto& s = *r->s;
    if (s.downloadDone.load()) return static_cast<int64_t>(s.bytes.load());
    // Estimate full stream size so mpv duration/seek math can use it.
    if (s.totalSegs > 0 && s.segsDone.load() > 0) {
        const double avg =
            static_cast<double>(s.bytes.load()) /
            static_cast<double>(s.segsDone.load());
        return static_cast<int64_t>(avg * static_cast<double>(s.totalSegs));
    }
    return MPV_ERROR_UNSUPPORTED;
}

void rClose(void* cookie) {
    auto* r = static_cast<Reader*>(cookie);
    if (!r) return;
    if (r->fp) fclose(r->fp);
    delete r;
}

void rCancel(void* cookie) {
    auto* r = static_cast<Reader*>(cookie);
    if (!r || !r->s) return;
    r->s->cancelled = true;
    r->s->cv.notify_all();
}

int openAni(void* /*user_data*/, char* uri, mpv_stream_cb_info* info) {
    const std::string id = uriToId(uri);
    auto s = findSession(id);
    if (!s) {
        char m[96];
        snprintf(m, sizeof(m), "SHLS: open miss id=%s", id.c_str());
        SHLOG(m);
        return MPV_ERROR_LOADING_FAILED;
    }
    auto* r = new Reader();
    r->s = s;
    r->pos = 0;
    r->fp = fopen(s->path.c_str(), "rb");
    if (!r->fp) {
        delete r;
        SHLOG("SHLS: open reader fp fail");
        return MPV_ERROR_LOADING_FAILED;
    }
    info->cookie = r;
    info->read_fn = rRead;
    info->seek_fn = rSeek;
    info->size_fn = rSize;
    info->close_fn = rClose;
    info->cancel_fn = rCancel;
    {
        char m[96];
        snprintf(m, sizeof(m), "SHLS: open ok id=%s", id.c_str());
        SHLOG(m);
    }
    return 0;
}

}  // namespace

void SeamlessHls::registerProtocol(void* mpvHandle) {
    if (!mpvHandle) return;
    auto* mpv = static_cast<mpv_handle*>(mpvHandle);
    const int rc = mpv_stream_cb_add_ro(mpv, "ani", nullptr, openAni);
    char m[80];
    snprintf(m, sizeof(m), "SHLS: register ani:// rc=%d", rc);
    SHLOG(m);
}

std::string SeamlessHls::start(std::vector<std::string> segUrls,
                                const std::string& cachePath,
                                const std::string& referer,
                                int totalDurationSec,
                                int totalSegs) {
    auto s = std::make_shared<Session>();
    s->id = std::to_string(g_nextId.fetch_add(1));
    s->path = cachePath;
    s->referer = referer;
    s->segs = std::move(segUrls);
    s->totalDurationSec = totalDurationSec;
    s->totalSegs = totalSegs > 0 ? totalSegs : static_cast<int>(s->segs.size());
    s->writer = fopen(cachePath.c_str(), "wb");
    if (!s->writer) {
        SHLOG("SHLS: cache open fail");
        return {};
    }
    {
        std::lock_guard<std::mutex> lk(g_mapMu);
        g_sessions[s->id] = s;
    }
    s->worker = std::thread(downloadWorker, s);
    return s->id;
}

void SeamlessHls::cancel(const std::string& id) {
    auto s = findSession(id);
    if (!s) return;
    s->cancelled = true;
    s->cv.notify_all();
    if (s->worker.joinable()) s->worker.detach();
    std::lock_guard<std::mutex> lk(s->mu);
    if (s->writer) {
        fclose(s->writer);
        s->writer = nullptr;
    }
}

bool SeamlessHls::isSeamlessUri(const std::string& uri) {
    return uri.rfind("ani://", 0) == 0;
}

void SeamlessHls::stats(const std::string& id, size_t* bytesOut,
                        size_t* segsOut, bool* doneOut,
                        int* totalDurationSecOut, int* totalSegsOut) {
    auto s = findSession(id);
    if (!s) return;
    if (bytesOut) *bytesOut = s->bytes.load();
    if (segsOut) *segsOut = s->segsDone.load();
    if (doneOut) *doneOut = s->downloadDone.load();
    if (totalDurationSecOut) *totalDurationSecOut = s->totalDurationSec;
    if (totalSegsOut) *totalSegsOut = s->totalSegs;
}

int SeamlessHls::parsePlaylistDurationSec(const std::string& m3u8Body) {
    double total = 0;
    std::istringstream iss(m3u8Body);
    std::string line;
    while (std::getline(iss, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
        if (line.rfind("#EXTINF:", 0) != 0) continue;
        const std::string v = line.substr(8);
        const auto comma = v.find(',');
        const std::string num = (comma == std::string::npos) ? v : v.substr(0, comma);
        try {
            total += std::stod(num);
        } catch (...) {
        }
    }
    return static_cast<int>(total + 0.5);
}

}  // namespace aniswitch
