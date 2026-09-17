// SPDX-License-Identifier: AGPL-3.0
//
// v22: TsVitch player graft — VideoView OSD + ani online download
// (mp4 single-file + HLS m3u8 segment concat).
#include "ui/activity/player_activity.hpp"
#include "ui/theme.hpp"
#include "player/tsvitch_video_view.hpp"
#include "player/mpv_core.hpp"
#include "core/episode_resolver.hpp"
#include "core/source_manager.hpp"
#include "net/http.hpp"
#include "utils/config_helper.hpp"
#include "utils/sqlite_store.hpp"
#include <borealis/core/logger.hpp>
#include <borealis/core/thread.hpp>
#include <cpr/cpr.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <fmt/format.h>

#if defined(__SWITCH__)
extern "C" void aniswitchStartupLog(const char*);
#define PLOG(m) aniswitchStartupLog(m)
#else
#define PLOG(m) do { (void)0; } while (0)
#endif

namespace aniswitch {

PlayerActivity::PlayerActivity(int32_t episodeId, const std::string&,
                               const std::string& videoSource,
                               int64_t resumePositionMs)
    : episodeId_(episodeId), videoSource_(videoSource),
      resumePositionMs_(resumePositionMs) {
    PLOG("player: ctor");
}

PlayerActivity::~PlayerActivity() {
    lifetime_.reset();
    try { MPVCore::instance().stop(); } catch (...) {}
}

void PlayerActivity::onContentAvailable() {
    PLOG("player: tsvitch onContentAvailable");
    video_ = new VideoView();
    setContentView(video_);
    PLOG("player: VideoView as contentView");
    video_->setTitle(fmt::format("播放 {}", episodeId_));
    video_->setVideoMode();
    video_->showLoading();
    video_->registerAction("返回", brls::BUTTON_B, [this](brls::View*) {
        brls::Application::popActivity();
        return true;
    });
    video_->registerAction("暂停", brls::BUTTON_X, [this](brls::View*) {
        video_->togglePlay();
        return true;
    });
    PLOG("player: tsvitch shell ready");

    if (!videoSource_.empty() && videoSource_ != "http" &&
        videoSource_ != "dandanplay" && videoSource_ != "myani") {
        startPlayback(videoSource_);
        return;
    }
    if (episodeId_ <= 0) {
        if (video_) video_->setCenterHintText("没有可播放的源");
        return;
    }
    if (video_) {
        video_->setCenterHintText("解析播放源…");
        video_->showLoading();
    }
    PLOG("player: resolve sources");
    std::weak_ptr<int> life = lifetime_;
    EpisodeResolver::resolve(
        0, episodeId_,
        [this, life](ResolvedEpisode result) {
            brls::sync([this, life, result] {
                if (life.expired()) return;
                subjectId_ = result.bangumiSubjectId;
                if (resumePositionMs_ == 0)
                    resumePositionMs_ = result.resumePositionMs;
                auto source =
                    SourceManager::instance().pickBest(result.sources);
                if (!source) {
                    if (video_) {
                        video_->hideLoading();
                        video_->setCenterHintText("没有可用在线源");
                    }
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
                startPlayback(source->url);
            });
        },
        [this, life](const std::string& msg, int) {
            brls::sync([this, life, msg] {
                if (life.expired()) return;
                if (video_) {
                    video_->hideLoading();
                    video_->setCenterHintText("解析失败: " + msg);
                }
            });
        });
}

void PlayerActivity::startPlayback(const std::string& url) {
    const bool network =
        url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0;
#if defined(__SWITCH__)
    // Switch mpv DNS cannot resolve Clash fake-ip / many CDNs.
    // ALL network videos go through our TLS/DNS stack, including HLS.
    if (network) {
        downloadThenPlay(url);
        return;
    }
#endif
    if (!video_) return;
    video_->showLoading();
    video_->setUrl(url);
    video_->invalidate();
    PLOG("player: tsvitch setUrl");
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
    {
        char b[200];
        snprintf(b, sizeof(b), "player: downloadThenPlay %s", url.c_str());
        PLOG(b);
    }
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
            cpr::Session s;
            HTTP::prepareFetchSession(s, url);
            s.SetTimeout(cpr::Timeout{180000});
            s.SetHeader(cpr::Header{{"User-Agent", "Mozilla/5.0"},
                                    {"Referer", "https://www.akianime.cc/"}});
            s.SetWriteCallback(cpr::WriteCallback{
                [&file, &total](std::string data, intptr_t) -> bool {
                    file.write(data.data(),
                               static_cast<std::streamsize>(data.size()));
                    total += data.size();
                    return true;
                }});
            auto r = s.Get();
            file.close();
            {
                char b[80];
                snprintf(b, sizeof(b), "player: dl status=%ld bytes=%zu",
                         r.status_code, total);
                PLOG(b);
            }
            if (r.error || r.status_code != 200 || total < 1024) {
                if (video_) {
                    video_->hideLoading();
                    video_->setCenterHintText(
                        fmt::format("下载失败 [{}]", r.status_code));
                }
                return;
            }
            PLOG("player: dl ok, play");
            if (video_) video_->hideLoading();
            startPlayback(out);
            return;
        }

        // HLS: playlist → .ts segments → local concat → play
        PLOG("player: hls get playlist");
        std::string body;
        if (!netFetch(url, body, &st)) {
            char b[80];
            snprintf(b, sizeof(b), "player: hls playlist fail st=%ld", st);
            PLOG(b);
            if (video_) {
                video_->hideLoading();
                video_->setCenterHintText(
                    fmt::format("m3u8 获取失败 [{}]", st));
            }
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
        {
            char b[80];
            snprintf(b, sizeof(b), "player: hls segments=%zu", segs.size());
            PLOG(b);
        }
        if (segs.empty()) {
            if (video_) {
                video_->hideLoading();
                video_->setCenterHintText("m3u8 无分片");
            }
            return;
        }
        // Cap first playable slice so Eden/Switch can show picture
        // without downloading a full 24-min episode first.
        size_t maxSegs = segs.size();
        if (maxSegs > 15) {
            maxSegs = 15;  // ~2 min — enough to prove picture in batch tests
            PLOG("player: hls cap first 15 segments");
        }
        for (size_t i = 0; i < maxSegs; ++i) {
            if (video_ && (i % 4 == 0)) {
                video_->setCenterHintText(
                    fmt::format("下载分片 {}/{}", i + 1, maxSegs));
            }
            std::string chunk;
            if (!netFetch(segs[i], chunk, &st) || chunk.empty()) {
                char b[96];
                snprintf(b, sizeof(b), "player: hls seg %zu fail st=%ld",
                         i, st);
                PLOG(b);
                continue;
            }
            file.write(chunk.data(),
                       static_cast<std::streamsize>(chunk.size()));
            total += chunk.size();
        }
        file.flush();
        file.close();
        {
            char b[80];
            snprintf(b, sizeof(b), "player: hls concat %zu bytes", total);
            PLOG(b);
        }
        if (total < 4096) {
            if (video_) {
                video_->hideLoading();
                video_->setCenterHintText("在线分片下载失败");
            }
            return;
        }
        PLOG("player: hls done, play");
        if (video_) {
            video_->hideLoading();
            video_->setCenterHintText(
                fmt::format("在线源 {:.1f} MB", total / 1048576.0));
        }
        startPlayback(out);
        if (video_) video_->invalidate();
    } catch (const std::exception& e) {
        char b[160];
        snprintf(b, sizeof(b), "player: dl exception %s", e.what());
        PLOG(b);
        if (video_) {
            video_->hideLoading();
            video_->setCenterHintText(std::string("下载异常: ") + e.what());
        }
    }
}
#else
void PlayerActivity::downloadThenPlay(const std::string& url) {
    startPlayback(url);
}
#endif

}  // namespace aniswitch
