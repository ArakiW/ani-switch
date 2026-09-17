// SPDX-License-Identifier: AGPL-3.0
#include "ui/activity/player_activity.hpp"
#include "player/mpv_core.hpp"
#include "player/video_view.hpp"
#include "player/subtitle_core.hpp"
#include "player/danmaku_renderer.hpp"
#include "core/episode_resolver.hpp"
#include "net/myani_client.hpp"
#include "net/dandanplay_client.hpp"
#include "net/dandan_types.hpp"
#include "net/danmaku_parser.hpp"
#include "utils/config_helper.hpp"
#include "utils/sqlite_store.hpp"
#include <borealis/core/thread.hpp>
#include <borealis/core/touch/tap_gesture.hpp>
#include <fstream>
#include <filesystem>
#include <fmt/format.h>

#if defined(__SWITCH__) && defined(ANISWITCH_SWITCH_DEBUG)
extern "C" void aniswitchStartupLog(const char*);
#define PLAYER_TRACE(msg) aniswitchStartupLog(msg)
#else
#define PLAYER_TRACE(msg) do { (void)0; } while (0)
#endif

namespace aniswitch {
PlayerActivity::PlayerActivity(int32_t episodeId, const std::string&, const std::string& videoSource,
                               int64_t resumePositionMs)
    : episodeId_(episodeId), videoSource_(videoSource), resumePositionMs_(resumePositionMs) {}

PlayerActivity::~PlayerActivity() {
    lifetime_.reset();
    if (!subscribed_) return;
    try { checkpoint(); } catch (const std::exception& e) { brls::Logger::error("Save progress failed: {}", e.what()); }
    MPVCore::instance().getEvent()->unsubscribe(subscription_);
    MPVCore::instance().stop();
    DanmakuRenderer::instance().clear();
}

void PlayerActivity::onContentAvailable() {
    auto& config = ProgramConfig::instance();
    MPVCore::HARDWARE_DEC = config.getIntOption(SettingItem::PLAYER_HWDEC) != 1;
    MPVCore::AUTO_PLAY = config.getBoolOption(SettingItem::PLAYER_AUTO_PLAY);
    MPVCore::VIDEO_VOLUME = config.getIntOption(SettingItem::PLAYER_VOLUME);
    DanmakuCore::DANMAKU_ON = config.getBoolOption(SettingItem::DANMAKU_ON);
    DanmakuCore::DANMAKU_SMART_MASK = false;
    DanmakuCore::DANMAKU_STYLE_ALPHA = config.getIntOption(SettingItem::DANMAKU_STYLE_ALPHA);
    DanmakuCore::DANMAKU_STYLE_FONTSIZE = config.getIntOption(SettingItem::DANMAKU_STYLE_FONTSIZE);
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setBackground(brls::ViewBackground::NONE);
    auto* video = new VideoView();
    video->setGrow(1);
    PLAYER_TRACE("player: VideoView created");
    // Touch UX: tap the video to pause / resume (the on-screen button
    // bar still exists for controller users; the tap gesture is for
    // handheld-mode touch).  The recognizer only fires on END (a
    // real tap, not a swipe), so the player isn't paused mid-swipe
    // when we eventually add seek-by-swipe.  brls's default tap
    // config highlights the view on press for visual feedback; the
    // MPV render surface is unaffected.
    video->addGestureRecognizer(new brls::TapGestureRecognizer(video,
        [this]() {
            auto& p = MPVCore::instance();
            if (p.isPaused()) p.resume();
            else p.pause();
        }));
    root->addView(video);
    status_ = new brls::Label();
    status_->setText("准备播放");
    status_->setTextColor(nvgRGB(255, 255, 255));
    status_->setBackgroundColor(nvgRGB(0, 0, 0));
    status_->setBackground(brls::ViewBackground::SHAPE_COLOR);
    root->addView(status_);
    auto* controls = new brls::Box();
    controls->setAxis(brls::Axis::ROW);
    auto add = [controls](const std::string& text, std::function<void()> action) {
        auto* button = new brls::Button();
        button->setText(text);
        button->registerClickAction([action](brls::View*) { action(); return true; });
        controls->addView(button);
    };
    add("暂停/播放", [] { auto& p = MPVCore::instance(); if (p.isPaused()) p.resume(); else p.pause(); });
    add("-10秒", [] { MPVCore::instance().seekRelative(-10); });
    add("+10秒", [] { MPVCore::instance().seekRelative(10); });
    add("字幕", [] {
        auto tracks = SubtitleCore::tracks();
        if (tracks.empty()) { brls::Application::notify("未发现字幕，可在视频旁放同名 .srt/.ass"); return; }
        for (size_t i = 0; i < tracks.size(); ++i) {
            if (!tracks[i].selected) continue;
            if (i + 1 < tracks.size()) SubtitleCore::select(tracks[i + 1].id);
            else SubtitleCore::disable();
            return;
        }
        SubtitleCore::select(tracks.front().id);
    });
    add("弹幕", [] { DanmakuCore::DANMAKU_ON = !DanmakuCore::DANMAKU_ON; });
    add("发弹幕", [this] {
        if (episodeId_ <= 0) {
            brls::Application::notify("当前没有关联 Bangumi 集数 ID，不能发弹幕");
            return;
        }
        // Capture the current playback time so the server-side timestamp
        // (currently 0 in our payload) gets anchored to "now" on display.
        const double now = MPVCore::instance().getPlaybackTime();
        brls::Application::getImeManager()->openForText(
            [this, now](std::string text) {
                if (text.empty()) return;
                DanmakuRenderer::instance().sendLocal(text);   // local echo
                MyaniClient::instance().sendDanmaku(
                    episodeId_, text, MyaniLocation::NORMAL, 0xFFFFFF,
                    [](MyaniDanmaku) {
                        brls::Application::notify("弹幕已发送");
                    },
                    [](const std::string& msg, int code) {
                        brls::Logger::warning("myani send failed [{}]: {}", code, msg);
                        brls::Application::notify("发送失败: " + msg);
                    });
            },
            "发弹幕", "", 200);
    });
    add("返回", [] { brls::Application::popActivity(); });
    root->addView(controls);
    setContentView(root);
    registerAction("暂停/播放", brls::BUTTON_X, [](brls::View*) {
        auto& player = MPVCore::instance();
        if (player.isPaused()) player.resume(); else player.pause();
        return true;
    });
    registerAction("返回", brls::BUTTON_B, [](brls::View*) { brls::Application::popActivity(); return true; });

    PLAYER_TRACE("player: MPVCore::instance begin");
    auto& player = MPVCore::instance();
    PLAYER_TRACE("player: MPVCore::instance done");
    player.command_async("set", "hwdec", MPVCore::HARDWARE_DEC ? MPVCore::PLAYER_HWDEC_METHOD : "no");
    player.command_async("set", "sub-auto", "fuzzy");
    player.setVolume(MPVCore::VIDEO_VOLUME);
    PLAYER_TRACE("player: commands sent");
    subscription_ = player.getEvent()->subscribe([this](MpvEventEnum event) { onPlayerEvent(event); });
    subscribed_ = true;
    PLAYER_TRACE("player: subscribed");
    if (videoSource_ != "http" && !videoSource_.empty()) { start(videoSource_); return; }
    std::weak_ptr<int> lifetime = lifetime_;
    auto prompt = [this, lifetime] {
        if (lifetime.expired()) return;
        status_->setText("请提供本地视频路径或 HTTP(S) 地址；条目信息不是视频源");
        brls::Application::getImeManager()->openForText([this, lifetime](std::string path) {
            if (!lifetime.expired() && !path.empty()) start(path);
        }, "视频地址或本地路径", "", 2048);
    };
    if (episodeId_ <= 0) { brls::sync(prompt); return; }
    EpisodeResolver::resolve(0, episodeId_, [this, lifetime, prompt](ResolvedEpisode result) {
        brls::sync([this, lifetime, prompt, result] {
            if (lifetime.expired()) return;
            subjectId_ = result.bangumiSubjectId;
            if (resumePositionMs_ == 0) resumePositionMs_ = result.resumePositionMs;
            auto source = SourceManager::instance().pickBest(result.sources);
            if (source) start(source->url); else prompt();
        });
    }, [this, lifetime, prompt](const std::string&, int) { brls::sync(prompt); });
}

void PlayerActivity::start(const std::string& path) {
    // sdmc:/switch/... is a local Switch path — strip the scheme so
    // std::filesystem and the :// check don't treat it as unsupported.
    std::string fsPath = path;
    if (fsPath.rfind("sdmc:", 0) == 0) fsPath = fsPath.substr(5);
    if (!fsPath.empty() && fsPath[0] != '/') fsPath = "/" + fsPath;

    const bool network = path.rfind("http://", 0) == 0 || path.rfind("https://", 0) == 0;
    if (!network && (fsPath.find("://") != std::string::npos || !std::filesystem::is_regular_file(fsPath))) {
        status_->setText("视频文件不存在: " + fsPath);
        brls::Logger::error("Player: local file missing: {}", fsPath);
        return;
    }
    const std::string playPath = network ? path : fsPath;
    videoSource_ = playPath;
    started_ = true;
    lastCheckpoint_ = -1;
    auto& player = MPVCore::instance();
    player.reset();
    DanmakuCore::instance().reset();
    player.setUrl(playPath);
    if (!network) {
        auto sidecar = std::filesystem::path(playPath).replace_extension(".xml");
        std::error_code error;
        auto size = std::filesystem::file_size(sidecar, error);
        if (!error && size > 16 * 1024 * 1024) {
            brls::Application::notify("弹幕 XML 超过 16MB，已跳过");
            return;
        }
        std::ifstream file(sidecar, std::ios::binary);
        if (file) {
            std::string xml((std::istreambuf_iterator<char>(file)), {});
            DanmakuRenderer::instance().loadHistorical(parseDandanXml(xml));
        }
    } else if (episodeId_ > 0) {
        // Network source: pull danmaku from BOTH myani and dandanplay
        // in parallel, so the timeline is more complete.  Both are
        // best-effort: any failure logs at debug level and playback
        // continues.
        std::weak_ptr<int> lifetime = lifetime_;
        auto& myani = MyaniClient::instance();
        myani.fetchDanmaku(episodeId_,
            [lifetime](std::vector<MyaniDanmaku> list) {
                if (lifetime.expired() || list.empty()) return;
                std::vector<ParsedDanmaku> converted;
                converted.reserve(list.size());
                for (const auto& m : list) converted.push_back(toParsed(m));
                DanmakuRenderer::instance().loadHistorical(converted);
                brls::Logger::info("myani: loaded {} danmaku for current episode", list.size());
            },
            [](const std::string& msg, int code) {
                brls::Logger::debug("myani fetch failed [{}]: {}", code, msg);
            });

        // dandanplay: take the last path component as the file name
        // and ask /api/v2/match to identify the corresponding anime
        // + episode.  If matched, fetch /api/v2/comment/{id} for the
        // historical danmaku timeline.  Both requests are async and
        // their results feed into the same dedup-aware loader.
        std::string fileName = path;
        const auto slash = fileName.find_last_of("/\\");
        if (slash != std::string::npos) fileName = fileName.substr(slash + 1);
        if (!fileName.empty()) {
            DandanplayClient::matchByFileName(fileName, 0, 0,
                [lifetime](DandanMatchResult m) {
                    if (lifetime.expired() || !m.matched || m.episodeId <= 0) return;
                    DandanplayClient::getComments(m.episodeId, /*withRelated=*/true, /*chConvert=*/0, /*mode=*/"",
                        [lifetime, epId = m.episodeId](std::vector<DandanComment> comments) {
                            if (lifetime.expired() || comments.empty()) return;
                            std::vector<ParsedDanmaku> parsed;
                            parsed.reserve(comments.size());
                            for (const auto& c : comments) {
                                ParsedDanmaku p;
                                p.time     = c.time;
                                p.text     = c.text;
                                p.mode     = c.mode;
                                p.color    = c.color;
                                p.fontSize = c.fontSize;
                                parsed.push_back(p);
                            }
                            DanmakuRenderer::instance().loadHistorical(parsed);
                            brls::Logger::info("dandanplay: loaded {} danmaku for dandanplay ep {}", comments.size(), epId);
                        },
                        [epId = m.episodeId](const std::string& msg, int code) {
                            brls::Logger::debug("dandanplay getComments({}) failed [{}]: {}", epId, code, msg);
                        });
                },
                [](const std::string& msg, int code) {
                    brls::Logger::debug("dandanplay match failed [{}]: {}", code, msg);
                });
        }
    }
    status_->setText("正在加载视频");
}

void PlayerActivity::onPlayerEvent(MpvEventEnum event) {
    auto& player = MPVCore::instance();
    if (event == MpvEventEnum::MPV_LOADED && resumePositionMs_ > 0) {
        player.seek(resumePositionMs_ / 1000);
        resumePositionMs_ = 0;
    }
    if (event == MpvEventEnum::MPV_FILE_ERROR) {
        status_->setText(fmt::format("播放失败: {}", mpv_error_string(player.mpv_error_code)));
    } else if (event == MpvEventEnum::UPDATE_PROGRESS || event == MpvEventEnum::MPV_LOADED) {
        status_->setText(fmt::format("{:.0f} / {} 秒", player.getPlaybackTime(), player.duration));
        auto second = static_cast<int64_t>(player.getPlaybackTime());
        if (second / 10 != lastCheckpoint_) { lastCheckpoint_ = second / 10; checkpoint(); }
    } else if (event == MpvEventEnum::LOADING_START) {
        status_->setText("正在缓冲");
    }
}

void PlayerActivity::checkpoint() {
    if (!started_ || episodeId_ <= 0) return;
    auto& player = MPVCore::instance();
    SQLiteStore::ProgressEntry entry;
    entry.episodeId = episodeId_; entry.subjectId = subjectId_;
    entry.positionMs = static_cast<int64_t>(player.getPlaybackTime() * 1000);
    entry.durationMs = player.duration * 1000;
    SQLiteStore::instance().upsertProgress(entry);
    SQLiteStore::HistoryEntry history;
    history.episodeId = episodeId_; history.subjectId = subjectId_;
    history.episodeName = videoSource_; history.positionMs = entry.positionMs; history.durationMs = entry.durationMs;
    // v17.5: stamp watchedAt so the new history view can group
    // entries by "今天 / 本周 / 更早".  time() is fine here — we
    // only need a per-second resolution for human-readable
    // bucketing, not for sorting (sort is by mtime in
    // upsertHistoryLocked).
    history.watchedAt = static_cast<int64_t>(time(nullptr));
    SQLiteStore::instance().upsertHistory(history);
}
void PlayerActivity::onPause() { if (subscribed_) { checkpoint(); MPVCore::instance().pause(); } }
}
