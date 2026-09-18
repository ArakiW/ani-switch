// SPDX-License-Identifier: AGPL-3.0
//
// v22: TsVitch player graft — VideoView OSD + ani online download
// (mp4 single-file + HLS m3u8 segment concat).
// v22.1: diagnostics + safe multi-core / memory / GPU fixes:
//   * PERF: breadcrumbs on open / mpv create / file load / play / dtor
//   * ImageLoader paused while player is open (reduce TLS + heap pressure)
//   * downloadThenPlay moved off the UI thread (was freezing the page)
//   * HLS / MP4 byte caps so a huge source cannot exhaust the 256MB heap
//   * never feed mpv a network URL on Switch (download-then-local only)
#include "ui/activity/player_activity.hpp"
#include "ui/theme.hpp"
#include "player/tsvitch_video_view.hpp"
#include "player/mpv_core.hpp"
#include "player/seamless_hls.hpp"
#include "core/episode_resolver.hpp"
#include "core/source_manager.hpp"
#include "net/http.hpp"
#include "utils/config_helper.hpp"
#include "utils/sqlite_store.hpp"
#include "utils/image_loader.hpp"
#include "utils/perf_switch.hpp"
#include "utils/thread_helper.hpp"
#include "utils/home_led.hpp"
#include <borealis/core/logger.hpp>
#include <borealis/core/application.hpp>
#include <borealis/core/thread.hpp>
#include <cpr/cpr.h>
#include <atomic>
#include <cstdarg>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <fmt/format.h>

#if defined(__SWITCH__)
extern "C" void aniswitchStartupLog(const char*);
#define PLOG(m) aniswitchStartupLog(m)
#else
#define PLOG(m) do { (void)0; } while (0)
#endif

namespace aniswitch {

namespace {
void plogf(const char* fmt, ...) {
#if defined(__SWITCH__)
    char b[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    aniswitchStartupLog(b);
#else
    (void)fmt;
#endif
}

// v22.1 hard caps (Switch heap is 256MB via __nx_heap_size; deko3d FBOs
// + mpv + UI chrome already take a large slice).
// v22.2: batch-test used 15 segs / 80MB (~1:12). Full episode needs the
// whole playlist on SD (typical anime ~24min ≈ 300 segs ≈ 0.5–2GB).
// Caps stay as safety rails against OOM, not as “demo length”.
constexpr size_t kHlsMaxSegments   = 500;
constexpr size_t kHlsMaxTotalBytes = 2048ull * 1024ull * 1024ull;  // 2 GiB
constexpr size_t kHlsMaxSegBytes   = 20ull * 1024ull * 1024ull;    // single seg
// Progressive: start mpv after this many local segments (~45–90s).
constexpr size_t kHlsStartSegments = 12;
}  // namespace

PlayerActivity::PlayerActivity(int32_t episodeId, const std::string&,
                               const std::string& videoSource,
                               int64_t resumePositionMs)
    : episodeId_(episodeId), videoSource_(videoSource),
      resumePositionMs_(resumePositionMs) {
    PLOG("player: open begin");
    perf::sample("player.ctor");
}

PlayerActivity::~PlayerActivity() {
    PLOG("player: dtor begin");
    lifetime_.reset();
    if (!seamlessId_.empty()) {
        SeamlessHls::cancel(seamlessId_);
        seamlessId_.clear();
    }
    MPVCore::HLS_PROGRESSIVE = false;
    MPVCore::KEEP_PLAYLIST = false;
    homeLedSetPlaybackScope(false);
    homeLedClear();
    try {
        // Unsubscribe VideoView before touching mpv so late wakeup
        // callbacks cannot land on a half-destroyed OSD.
        if (video_) {
            video_->unRegisterMpvEvent();
            PLOG("player: VideoView events unregistered");
        }
        MPVCore::instance().stop();
        PLOG("player: mpv stop on pop");
    } catch (...) {
        PLOG("player: dtor stop exception");
    }
    video_ = nullptr;
    try {
        ImageLoader::instance().setPaused(false);
        PLOG("player: image loader resumed");
    } catch (...) {
        PLOG("player: dtor image-loader resume exception");
    }
    perf::sample("player.dtor");
    PLOG("player: dtor done");
}

void PlayerActivity::onContentAvailable() {
    PLOG("player: tsvitch onContentAvailable");
    homeLedSetPlaybackScope(true);
    homeLedSetLoading();
    try {
        // Pause cover / rail image downloads for the lifetime of the
        // player.  Serial ImageLoader + player TLS + mpv init on the
        // same Switch network stack was a known crash cluster.
        ImageLoader::instance().setPaused(true);
        ImageLoader::instance().clearPending();
        PLOG("player: image loader paused + cleared");

#if defined(__SWITCH__)
        // Belt-and-braces: ProgramConfig::init already forces these,
        // but MPVCore is a lazy singleton — re-assert immediately before
        // first touch in case another path constructed it first.
        MPVCore::HARDWARE_DEC = true;
        MPVCore::PLAYER_HWDEC_METHOD = "auto";
        MPVCore::INMEMORY_CACHE = 0;
#endif
        perf::beginLoadWindow("player.open");

        PLOG("player: mpv create begin");
        // Force singleton construction here so breadcrumbs bracket
        // mpv_create / mpv_initialize / render-context create.
        auto& mpv = MPVCore::instance();
        (void)mpv;
        PLOG("player: mpv init done");
        perf::sample("player.after_mpv");

        video_ = new VideoView();
        setContentView(video_);
        PLOG("player: VideoView as contentView");
        video_->setTitle(fmt::format("播放 {}", episodeId_));
        video_->setVideoMode();
        video_->showLoading();
        video_->registerAction("返回", brls::BUTTON_B, [this](brls::View*) {
            PLOG("player: back pressed");
            brls::Application::popActivity();
            return true;
        });
        video_->registerAction("暂停", brls::BUTTON_X, [this](brls::View*) {
            try {
                video_->togglePlay();
            } catch (...) {
                PLOG("player: togglePlay exception");
            }
            return true;
        });
        PLOG("player: tsvitch shell ready");

        if (!videoSource_.empty() && videoSource_ != "http" &&
            videoSource_ != "dandanplay" && videoSource_ != "myani") {
            // Local path (or already-resolved non-network source).
            startPlayback(videoSource_);
            return;
        }
        if (episodeId_ <= 0) {
            if (video_) {
                video_->setCenterHintText(
                    episodeId_ < 0
                        ? "演示/本地条目：请用本地视频路径打开播放器"
                        : "没有可播放的源（无效集数 ID）");
            }
            plogf("player: resolve skip ep=%d (<=0)", episodeId_);
            perf::endLoadWindow("player.open.no_source");
            return;
        }
        if (video_) {
            video_->setCenterHintText("解析播放源…");
            video_->showLoading();
        }
        plogf("player: RESOLVE begin ep=%d", episodeId_);
        PLOG("player: resolve sources");
        std::weak_ptr<int> life = lifetime_;
        EpisodeResolver::resolve(
            0, episodeId_,
            [this, life](ResolvedEpisode result) {
                brls::sync([this, life, result] {
                    if (life.expired()) return;
                    try {
                        plogf("player: RESOLVE got n=%zu ep=%d sid=%d",
                              result.sources.size(), result.episodeId,
                              result.bangumiSubjectId);
                        subjectId_ = result.bangumiSubjectId;
                        if (resumePositionMs_ == 0)
                            resumePositionMs_ = result.resumePositionMs;
                        auto source =
                            SourceManager::instance().pickBest(result.sources);
                        if (!source) {
                            if (video_) {
                                video_->hideLoading();
                                const std::string note =
                                    result.resolveNote.empty()
                                        ? std::string("sources.json 无映射，在线源为空")
                                        : result.resolveNote;
                                video_->setCenterHintText(fmt::format(
                                    "没有可用在线源\nep={} sid={}\n{}",
                                    result.episodeId ? result.episodeId
                                                     : episodeId_,
                                    result.bangumiSubjectId, note));
                            }
                            plogf("player: RESOLVE empty sources ep=%d",
                                  episodeId_);
                            perf::endLoadWindow("player.open.no_best");
                            return;
                        }
                        fallbackUrls_.clear();
                        for (const auto& s : result.sources) {
                            if (s.url != source->url && !s.url.empty())
                                fallbackUrls_.push_back(s.url);
                        }
                        if (video_) {
                            video_->setTitle(source->label.empty()
                                                 ? result.title
                                                 : source->label);
                            video_->setCenterHintText("加载中…");
                        }
                        {
                            char b[160];
                            snprintf(b, sizeof(b),
                                     "player: resolve picked ep=%d",
                                     episodeId_);
                            PLOG(b);
                        }
                        startPlayback(source->url);
                    } catch (const std::exception& e) {
                        plogf("player: resolve cb exception %s", e.what());
                        if (video_) {
                            video_->hideLoading();
                            video_->setCenterHintText(
                                std::string("播放启动失败: ") + e.what());
                        }
                        try {
                            brls::Application::notify(
                                std::string("播放启动失败: ") + e.what());
                        } catch (...) {}
                    } catch (...) {
                        PLOG("player: resolve cb unknown exception");
                        if (video_) {
                            video_->hideLoading();
                            video_->setCenterHintText("播放启动失败");
                        }
                    }
                });
            },
            [this, life](const std::string& msg, int code) {
                brls::sync([this, life, msg, code] {
                    if (life.expired()) return;
                    plogf("player: RESOLVE fail code=%d ep=%d msg=%.80s", code,
                          episodeId_, msg.c_str());
                    // Classify — EpisodeResolver already returns specific
                    // Chinese text; prefix a short kind tag for OSD.
                    const char* kind = "解析失败";
                    if (msg.find("演示") != std::string::npos) kind = "演示数据";
                    else if (msg.find("超时") != std::string::npos) kind = "网络超时";
                    else if (msg.find("连接") != std::string::npos) kind = "网络连接失败";
                    else if (msg.find("404") != std::string::npos) kind = "集数 ID 无效";
                    else if (msg.find("TLS") != std::string::npos ||
                             msg.find("SSL") != std::string::npos) kind = "TLS 错误";
                    if (video_) {
                        video_->hideLoading();
                        video_->setCenterHintText(
                            fmt::format("{}\n{}", kind, msg));
                    }
                    try {
                        brls::Application::notify(fmt::format("{}: {}", kind, msg));
                    } catch (...) {}
                    perf::endLoadWindow("player.open.resolve_fail");
                });
            });
    } catch (const std::exception& e) {
        plogf("player: onContentAvailable exception %s", e.what());
        PLOG("player: open FAILED");
        try {
            if (video_) {
                video_->hideLoading();
                video_->setCenterHintText(
                    std::string("播放器打开失败: ") + e.what());
            }
            brls::Application::notify(
                std::string("播放器打开失败: ") + e.what());
        } catch (...) {}
        try { ImageLoader::instance().setPaused(false); } catch (...) {}
    } catch (...) {
        PLOG("player: onContentAvailable unknown exception");
        try {
            brls::Application::notify("播放器打开失败");
        } catch (...) {}
        try { ImageLoader::instance().setPaused(false); } catch (...) {}
    }
}

void PlayerActivity::startPlayback(const std::string& url) {
    const bool network =
        url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0;
#if defined(__SWITCH__)
    // Experiment mpv-direct (wiliwili-style): allow network loadfile.
    // Default seamless: ALL network videos download via our HTTP stack.
    if (network && !MPVCore::ALLOW_NETWORK_URL) {
        downloadThenPlay(url);
        return;
    }
    if (network && MPVCore::ALLOW_NETWORK_URL) {
        PLOG("player: mpv-direct network loadfile");
        homeLedSetLoading();
    }
#endif
    {
        char b[200];
        snprintf(b, sizeof(b), "player: file load begin %s", url.c_str());
        PLOG(b);
    }
    if (!video_) return;
    try {
        video_->showLoading();
        MPVCore::AUTO_PLAY = true;  // never leave local/open path paused
        video_->setUrl(url);  // VideoView adds referer/proxy extra for http
        video_->invalidate();
        PLOG("player: tsvitch setUrl");
    } catch (const std::exception& e) {
        plogf("player: setUrl exception %s", e.what());
        if (video_) {
            video_->hideLoading();
            video_->setCenterHintText(std::string("加载失败: ") + e.what());
        }
        try {
            brls::Application::notify(std::string("加载失败: ") + e.what());
        } catch (...) {}
    } catch (...) {
        PLOG("player: setUrl unknown exception");
        if (video_) {
            video_->hideLoading();
            video_->setCenterHintText("加载失败");
        }
    }
}

#if defined(__SWITCH__)
namespace {
bool netFetch(const std::string& url, std::string& out, long* status) {
    cpr::Session s;
    HTTP::prepareFetchSession(s, url);
    s.SetTimeout(cpr::Timeout{90000});
    s.SetConnectTimeout(cpr::ConnectTimeout{15000});
    s.SetHeader(cpr::Header{
        {"User-Agent",
         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"},
        {"Referer", "https://www.akianime.cc/"},
        {"Accept", "*/*"},
    });
    auto r = s.Get();
    if (status) *status = r.status_code;
    if (r.error || r.status_code != 200) return false;
    out = std::move(r.text);
    return !out.empty();
}

std::string urlJoin(const std::string& base, const std::string& rel) {
    if (rel.rfind("http://", 0) == 0 || rel.rfind("https://", 0) == 0)
        return rel;
    if (!rel.empty() && rel[0] == '/') {
        auto s = base.find("://");
        if (s == std::string::npos) return rel;
        auto h = base.find('/', s + 3);
        return base.substr(0, h == std::string::npos ? base.size() : h) + rel;
    }
    auto slash = base.find_last_of('/');
    if (slash == std::string::npos) return base + "/" + rel;
    return base.substr(0, slash + 1) + rel;
}
}  // namespace

void PlayerActivity::downloadThenPlay(const std::string& url) {
    if (video_) {
        video_->setCenterHintText("在线获取中…");
        video_->showLoading();
    }
    plogf("player: downloadThenPlay %s", url.c_str());
    homeLedSetLoading();

    // v22.1: run the download off the UI thread.  Previously this ran
    // synchronously on the borealis main thread, which froze the player
    // page for the whole HTTP transfer ("卡住") and starved mpv + input.
    std::weak_ptr<int> life = lifetime_;
    auto safeHint = [this, life](const std::string& text) {
        brls::sync([this, life, text] {
            if (life.expired() || !video_) return;
            try {
                video_->setCenterHintText(text);
            } catch (...) {}
        });
    };
    auto safeHideLoading = [this, life]() {
        brls::sync([this, life] {
            if (life.expired() || !video_) return;
            try { video_->hideLoading(); } catch (...) {}
        });
    };

    submit_detached([this, life, url, safeHint, safeHideLoading]() {
        try {
            const bool isHls = url.find(".m3u8") != std::string::npos;
            uint64_t h = 1469598103934665603ULL;
            for (unsigned char c : url) { h ^= c; h *= 1099511628211ULL; }
            const std::string out = fmt::format(
                "sdmc:/switch/aniswitch/vc_{:016x}{}", h, isHls ? ".ts" : ".mp4");
            std::ofstream file(out, std::ios::binary | std::ios::trunc);
            if (!file) throw std::runtime_error("cache open failed");
            size_t total = 0;
            long st = 0;

            if (!isHls) {
                // Stream-to-disk: never buffer the whole body in RAM.
                cpr::Session s;
                HTTP::prepareFetchSession(s, url);
                s.SetTimeout(cpr::Timeout{180000});
                s.SetHeader(cpr::Header{{"User-Agent", "Mozilla/5.0"},
                                        {"Referer", "https://www.akianime.cc/"}});
                s.SetWriteCallback(cpr::WriteCallback{
                    [&file, &total, &life](std::string data, intptr_t) -> bool {
                        if (life.expired()) return false;  // abort transfer
                        file.write(data.data(),
                                   static_cast<std::streamsize>(data.size()));
                        total += data.size();
                        return true;
                    }});
                auto r = s.Get();
                file.close();
                plogf("player: dl status=%ld bytes=%zu", r.status_code, total);
                if (life.expired()) {
                    PLOG("player: dl aborted (activity gone)");
                    return;
                }
                if (r.error || r.status_code != 200 || total < 1024) {
                    homeLedSetError();
                    safeHideLoading();
                    safeHint(fmt::format("下载失败 [{}]", r.status_code));
                    try {
                        brls::sync([this, life, code = r.status_code] {
                            if (life.expired()) return;
                            try {
                                brls::Application::notify(
                                    fmt::format("下载失败 [{}]", code));
                            } catch (...) {}
                        });
                    } catch (...) {}
                    perf::endLoadWindow("player.dl.fail");
                    return;
                }
                PLOG("player: dl ok, play local");
                perf::sample("player.dl.ok");
                homeLedSetOk();
                brls::sync([this, life, out] {
                    if (life.expired()) return;
                    if (video_) video_->hideLoading();
                    startPlayback(out);
                });
                return;
            }

            // HLS: either mpv-direct (wiliwili) or seamless ani:// local stream.
            PLOG("player: hls get playlist");
            std::string body;
            if (!netFetch(url, body, &st)) {
                plogf("player: hls playlist fail st=%ld", st);
                if (life.expired()) return;
                homeLedSetError();
                safeHideLoading();
                safeHint(fmt::format("m3u8 获取失败 [{}]", st));
                perf::endLoadWindow("player.hls.playlist_fail");
                return;
            }
            std::vector<std::string> segs;
            std::istringstream iss(body);
            std::string line;
            while (std::getline(iss, line)) {
                while (!line.empty() &&
                       (line.back() == '\r' || line.back() == '\n'))
                    line.pop_back();
                if (line.empty() || line[0] == '#') continue;
                segs.push_back(urlJoin(url, line));
            }
            plogf("player: hls segments=%zu", segs.size());
            if (segs.empty()) {
                safeHideLoading();
                safeHint("m3u8 无分片");
                return;
            }
            const int playlistDur = SeamlessHls::parsePlaylistDurationSec(body);
            plogf("player: hls playlist duration=%d sec mode=%s", playlistDur,
                  MPVCore::ALLOW_NETWORK_URL ? "mpv-direct" : "seamless");

            // ---- Experiment: wiliwili-style mpv direct network HLS ----
            if (MPVCore::ALLOW_NETWORK_URL) {
                PLOG("player: hls mpv-direct loadfile network m3u8");
                file.close();
                brls::sync([this, life, url, playlistDur] {
                    if (life.expired()) return;
                    MPVCore::AUTO_PLAY = true;
                    MPVCore::HLS_PROGRESSIVE = false;
                    MPVCore::KEEP_PLAYLIST = false;
                    if (video_) {
                        if (playlistDur > 0) video_->setRealDuration(playlistDur);
                        video_->setCenterHintText(
                            playlistDur > 0
                                ? fmt::format("mpv 直连 · 总长 {}:{:02d}",
                                              playlistDur / 60, playlistDur % 60)
                                : "mpv 直连加载中…");
                    }
                    startPlayback(url);
                    if (video_) video_->invalidate();
                });
                // Poll until playing or timeout for LED/OSD.
                for (int i = 0; i < 200; ++i) {
                    if (life.expired()) return;
                    auto& mpv = MPVCore::instance();
                    if (!mpv.video_stopped &&
                        (mpv.playback_time > 0.2 || mpv.duration > 0 ||
                         mpv.video_playing)) {
                        homeLedSetOk();
                        plogf("player: hls mpv-direct live time=%.2f dur=%lld",
                              mpv.playback_time, (long long)mpv.duration);
                        if (video_ && playlistDur > 0)
                            video_->setRealDuration(playlistDur);
                        break;
                    }
                    if (i == 80) {
                        plogf("player: hls mpv-direct still waiting t=%d stopped=%d",
                              i, mpv.video_stopped ? 1 : 0);
                    }
                    if (i >= 150) {
                        homeLedSetError();
                        plogf("player: hls mpv-direct timeout/failed stopped=%d",
                              mpv.video_stopped ? 1 : 0);
                        brls::sync([this, life] {
                            if (life.expired() || !video_) return;
                            video_->setCenterHintText(
                                "mpv 直连失败（DNS/代理）— 请改回无缝流");
                        });
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
                return;
            }

            size_t maxSegs = segs.size();
            if (maxSegs > kHlsMaxSegments) {
                maxSegs = kHlsMaxSegments;
                plogf("player: hls cap segments=%zu (playlist=%zu)", maxSegs,
                      segs.size());
            }
            segs.resize(maxSegs);
            file.close();  // seamless writer owns the cache file

            const std::string cachePath = fmt::format(
                "sdmc:/switch/aniswitch/vc_{:016x}.ts", h);
            const std::string referer = "https://www.akianime.cc/";
            const std::string sid = SeamlessHls::start(
                segs, cachePath, referer, playlistDur,
                static_cast<int>(maxSegs));
            if (sid.empty()) {
                safeHideLoading();
                safeHint("无缝流会话创建失败");
                return;
            }
            seamlessId_ = sid;
            plogf("player: hls seamless session id=%s dur=%d", sid.c_str(),
                  playlistDur);
            MPVCore::HLS_PROGRESSIVE = true;
            MPVCore::KEEP_PLAYLIST = true;
            homeLedSetLoading();

            // Start mpv after a few MB so demuxer has probe data.
            constexpr size_t kSeamlessStartBytes = 3ull * 1024ull * 1024ull;
            const std::string uri = "ani://" + sid;
            for (int i = 0; i < 400; ++i) {  // ~60s max wait for first buffer
                if (life.expired()) {
                    SeamlessHls::cancel(sid);
                    return;
                }
                size_t bytes = 0, nseg = 0;
                bool done = false;
                SeamlessHls::stats(sid, &bytes, &nseg, &done);
                if (i % 5 == 0) {
                    safeHint(fmt::format("无缝缓冲 {:.1f}MB / {} 段…",
                                         bytes / 1048576.0, nseg));
                }
                if (bytes >= kSeamlessStartBytes || (done && bytes > 4096)) {
                    PLOG("player: hls seamless start playback");
                    brls::sync([this, life, uri, bytes, playlistDur] {
                        if (life.expired()) return;
                        MPVCore::AUTO_PLAY = true;
                        if (video_) {
                            video_->hideLoading();
                            // wiliwili REAL_DURATION: progress bar uses full
                            // playlist length, not mpv's buffered duration.
                            if (playlistDur > 0)
                                video_->setRealDuration(playlistDur);
                            video_->setCenterHintText(fmt::format(
                                "无缝播放 · 缓冲 {:.1f}MB", bytes / 1048576.0));
                        }
                        startPlayback(uri);
                        if (video_) video_->invalidate();
                    });
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
            }

            // Background OSD until download completes.
            for (int i = 0; i < 2400; ++i) {
                if (life.expired()) return;
                size_t bytes = 0, nseg = 0;
                bool done = false;
                int totalDur = playlistDur;
                int totalSegs = static_cast<int>(maxSegs);
                SeamlessHls::stats(sid, &bytes, &nseg, &done, &totalDur,
                                    &totalSegs);
                if (done) {
                    plogf("player: hls seamless done bytes=%zu segs=%zu dur=%d",
                          bytes, nseg, totalDur);
                    perf::sample("player.hls.seamless_ok");
                    homeLedSetOk();
                    brls::sync([this, life, bytes, nseg, totalDur] {
                        if (life.expired() || !video_) return;
                        if (totalDur > 0) video_->setRealDuration(totalDur);
                        video_->setCenterHintText(fmt::format(
                            "已完整缓存 {} 段 · {:.1f}MB", nseg,
                            bytes / 1048576.0));
                    });
                    break;
                }
                if (i % 20 == 0) {
                    brls::sync([this, life, bytes, nseg, maxSegs, totalDur] {
                        if (life.expired() || !video_) return;
                        if (totalDur > 0) video_->setRealDuration(totalDur);
                        video_->setCenterHintText(fmt::format(
                            "后台下载 {}/{} 段 · {:.1f}MB", nseg, maxSegs,
                            bytes / 1048576.0));
                    });
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        } catch (const std::exception& e) {
            plogf("player: dl exception %s", e.what());
            if (life.expired()) return;
            homeLedSetError();
            safeHideLoading();
            safeHint(std::string("下载异常: ") + e.what());
            try {
                brls::sync([this, life, what = std::string(e.what())] {
                    if (life.expired()) return;
                    try {
                        brls::Application::notify("下载异常: " + what);
                    } catch (...) {}
                });
            } catch (...) {}
            perf::endLoadWindow("player.dl.exception");
        } catch (...) {
            PLOG("player: dl unknown exception");
            if (life.expired()) return;
            homeLedSetError();
            safeHideLoading();
            safeHint("下载异常");
            perf::endLoadWindow("player.dl.unknown");
        }
    });
}
#else
void PlayerActivity::downloadThenPlay(const std::string& url) {
    homeLedSetLoading();
    startPlayback(url);
}
#endif

}  // namespace aniswitch
