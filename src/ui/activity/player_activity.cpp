// SPDX-License-Identifier: AGPL-3.0
//
// v22: TsVitch player graft — VideoView OSD + ani online download.
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
#include <filesystem>
#include <fstream>
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
    // Always build TsVitch VideoView in C++ — XML activity shell only
    // hosts an empty Box so inflate cannot abort before we get here.
    auto* box = new brls::Box();
    box->setAxis(brls::Axis::COLUMN);
    box->setBackgroundColor(nvgRGB(0, 0, 0));
#ifdef __SWITCH__
    box->setWidth(theme::kDesignWidth);
#endif
    video_ = new VideoView();
    video_->setGrow(1.0f);
    box->addView(video_);
    setContentView(box);
    PLOG("player: VideoView created");
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
    // Explicit URL → play immediately.
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
    EpisodeResolver::resolve(0, episodeId_,
        [this, life](ResolvedEpisode result) {
            brls::sync([this, life, result] {
                if (life.expired()) return;
                subjectId_ = result.bangumiSubjectId;
                if (resumePositionMs_ == 0) resumePositionMs_ = result.resumePositionMs;
                auto source = SourceManager::instance().pickBest(result.sources);
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
                    video_->setTitle(source->label.empty() ? result.title : source->label);
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
    // mpv cannot resolve Clash fake-ip; download via our TLS/DNS stack.
    if (network && url.find(".m3u8") == std::string::npos) {
        downloadThenPlay(url);
        return;
    }
#endif
    if (!video_) return;
    video_->showLoading();
    video_->setUrl(url);
    PLOG("player: tsvitch setUrl");
}

#if defined(__SWITCH__)
void PlayerActivity::downloadThenPlay(const std::string& url) {
    if (video_) {
        video_->setCenterHintText("在线下载中…");
        video_->showLoading();
    }
    PLOG("player: downloadThenPlay");
    try {
        uint64_t h = 1469598103934665603ULL;
        for (unsigned char c : url) { h ^= c; h *= 1099511628211ULL; }
        const std::string out =
            fmt::format("sdmc:/switch/aniswitch/vc_{:016x}.mp4", h);
        cpr::Session s;
        HTTP::prepareFetchSession(s, url);
        s.SetTimeout(cpr::Timeout{180000});
        s.SetConnectTimeout(cpr::ConnectTimeout{20000});
        std::ofstream file(out, std::ios::binary | std::ios::trunc);
        size_t written = 0;
        s.SetWriteCallback(cpr::WriteCallback{
            [&file, &written](std::string data, intptr_t) -> bool {
                if (!file) return false;
                file.write(data.data(), static_cast<std::streamsize>(data.size()));
                written += data.size();
                return true;
            }});
        PLOG("player: dl get");
        auto r = s.Get();
        file.flush();
        file.close();
        std::error_code ec;
        const auto fsize = std::filesystem::exists(out, ec)
                               ? std::filesystem::file_size(out, ec) : 0;
        {
            char b[180];
            snprintf(b, sizeof(b), "player: dl status=%ld bytes=%zu",
                     r.status_code, written);
            PLOG(b);
        }
        if (r.error || r.status_code != 200 || fsize < 1024) {
            if (!fallbackUrls_.empty()) {
                const std::string next = fallbackUrls_.front();
                fallbackUrls_.erase(fallbackUrls_.begin());
                startPlayback(next);
            } else if (video_) {
                video_->hideLoading();
                video_->setCenterHintText(fmt::format("下载失败 [{}]", r.status_code));
            }
            return;
        }
        PLOG("player: dl ok, play");
        if (video_) video_->hideLoading();
        startPlayback(out);
    } catch (const std::exception& e) {
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
