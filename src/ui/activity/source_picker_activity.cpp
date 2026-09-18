// SPDX-License-Identifier: AGPL-3.0
//
// v22 chrome: page root (kChromeBg + TV-safe margins + Switch width),
// title kTypeH2 / status kTypeCaption on theme tokens, 88px source rows
// + focus, HUD shell + B 返回 via chrome helpers.
// Episode → resolve → source → play logic unchanged (no presenter/HTTP edits).
//
// v22.2 episode-resolve: classified error text (no bare 解析失败),
// RESOLVE: logs with ep/sid/nSources, demo ids never claim live parse,
// autoPickFirst empty list is "no sources" not "parse fail".

#include "ui/activity/source_picker_activity.hpp"
#include "ui/ui_chrome.hpp"
#include "core/episode_resolver.hpp"
#include "core/http_source.hpp"
#include "ui/hud.hpp"
#include "ui/theme.hpp"
#include "ui/demo_data.hpp"
#include "utils/activity_helper.hpp"
#include <borealis/core/logger.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/scrolling_frame.hpp>
#include <algorithm>
#include <cstdarg>
#include <fmt/format.h>
#include <string>

#if defined(__SWITCH__)
extern "C" void aniswitchStartupLog(const char* message);
#define PICKLOG(m) aniswitchStartupLog(m)
#else
#define PICKLOG(m) do { (void)0; } while (0)
#endif

namespace aniswitch {

namespace {
void pickLogf(const char* fmt, ...) {
#if defined(__SWITCH__)
    char b[200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    aniswitchStartupLog(b);
#else
    (void)fmt;
#endif
}
}  // namespace

SourcePickerActivity::SourcePickerActivity(int32_t episodeId,
                                           int32_t subjectId,
                                           const std::string& title,
                                           bool autoPickFirst)
    : episodeId_(episodeId), subjectId_(subjectId), title_(title),
      autoPickFirst_(autoPickFirst) {}

void SourcePickerActivity::onContentAvailable() {
    // Padded chrome page as scroll content.
    auto* content = chrome::makePageRoot();

    content->addView(chrome::makeTitle(
        title_.empty() ? fmt::format("选择播放源 · EP{}", episodeId_)
                       : fmt::format("选择播放源 · {}", title_),
        theme::kTypeH2, 8));

    status_ = new brls::Label();
    status_->setText("正在解析播放源…");
    status_->setFontSize(theme::kTypeCaption);
    status_->setTextColor(theme::kDarkTextMuted);
    status_->setMarginBottom(16);
    content->addView(status_);

    list_ = new brls::Box();
    list_->setAxis(brls::Axis::COLUMN);
    content->addView(list_);

    auto* shell = chrome::attachScrollShell(this, content);
    if (shell) shell->setBackgroundColor(theme::kChromeBg);
    chrome::registerBack(this);
    chrome::finish(this, shell);

    pickLogf("RESOLVE: picker open ep=%d sid=%d autoPick=%d",
             episodeId_, subjectId_, autoPickFirst_ ? 1 : 0);

    // v22: demo / negative ids — skip network, offer local sample.
    // Never claim a live API parse for sentinel ids.
    if (episodeId_ < 0) {
        VideoSource local;
        local.kind = SourceKind::CACHE;
        local.url = "sdmc:/switch/aniswitch/videos/test-local.mp4";
        local.label = "本地测试视频（演示数据，非在线解析）";
        local.priority = 0;
        std::vector<VideoSource> demoSources{local};
        PICKLOG("RESOLVE: picker demo-id → local sample");
        renderSources(demoSources);
        return;
    }
    if (episodeId_ == 0) {
        setLoading("无效的集数 ID（0），无法解析。\n"
                   "请从剧集列表重新选择，或播放本地视频。");
        return;
    }

    const bool jsonOk = HTTPSourceProvider::manifestOk();
    const auto jsonKeys = HTTPSourceProvider::manifestKeyCount();
    const std::string jsonPath = HTTPSourceProvider::manifestPath();
    setLoading(fmt::format(
        "正在解析播放源…（ep={} sid={}）\n"
        "sources.json: {}\n"
        "在线检索 / 本地清单",
        episodeId_, subjectId_,
        jsonOk ? fmt::format("已加载 {} 个 key @ {}", jsonKeys, jsonPath)
               : fmt::format("不可用（{}）@ {}",
                             HTTPSourceProvider::lastError().empty()
                                 ? std::string("缺失")
                                 : HTTPSourceProvider::lastError(),
                             jsonPath)));

#if defined(__SWITCH__)
    aniswitchStartupLog("PICKER: resolve begin");
#endif
    std::weak_ptr<int> life = lifetime_;
    EpisodeResolver::resolve(subjectId_, episodeId_,
        [this, life](ResolvedEpisode result) {
            brls::sync([this, life, result] {
                if (life.expired()) return;
                pickLogf("RESOLVE: picker got n=%zu ep=%d sid=%d",
                         result.sources.size(), result.episodeId,
                         result.bangumiSubjectId);
                if (result.sources.empty()) {
                    // Empty after a complete resolve is "no sources",
                    // NOT a parse failure. Include ep/sid + provenance.
                    const std::string note = result.resolveNote.empty()
                                                 ? std::string("未解析到可播放 URL")
                                                 : result.resolveNote;
                    setLoading(fmt::format(
                        "没有找到播放源。\n"
                        "ep={} sid={}\n"
                        "{}\n"
                        "· sources.json key 是 Bangumi 集数 ID\n"
                        "· 可检查网络/代理，或返回后从本地视频播放",
                        result.episodeId ? result.episodeId : episodeId_,
                        result.bangumiSubjectId ? result.bangumiSubjectId
                                                : subjectId_,
                        note));
                    return;
                }
                renderSources(result.sources);
            });
        },
        [this, life](const std::string& msg, int code) {
            brls::sync([this, life, msg, code] {
                if (life.expired()) return;
                pickLogf("RESOLVE: picker error code=%d msg=%s", code,
                         msg.c_str());
                // Classify: demo / no sources / network / parse — msg from
                // EpisodeResolver is already Chinese and specific.
                const char* kind = "解析失败";
                if (msg.find("演示") != std::string::npos) kind = "演示数据";
                else if (msg.find("超时") != std::string::npos) kind = "网络超时";
                else if (msg.find("连接") != std::string::npos) kind = "网络连接失败";
                else if (msg.find("解析失败") != std::string::npos &&
                         msg.find("域名") != std::string::npos) kind = "DNS 失败";
                else if (msg.find("404") != std::string::npos) kind = "集数 ID 无效";
                else if (msg.find("无") != std::string::npos &&
                         msg.find("源") != std::string::npos) kind = "无播放源";
                else if (msg.find("TLS") != std::string::npos ||
                         msg.find("SSL") != std::string::npos) kind = "TLS 错误";
                setLoading(fmt::format("{}\n{}\n（ep={} sid={} code={}）", kind,
                                       msg, episodeId_, subjectId_, code));
            });
        });
}

void SourcePickerActivity::setLoading(const std::string& msg) {
    if (status_) {
        status_->setText(msg);
        status_->setSingleLine(false);
    }
}

void SourcePickerActivity::renderSources(std::vector<VideoSource> sources) {
    if (!list_) return;
    list_->clearViews();
    if (status_) {
        status_->setText(fmt::format("共 {} 个播放源，请选择（ep={}）",
                                     sources.size(), episodeId_));
        status_->setTextColor(theme::kDarkTextSecondary);
    }

    // Sort like pickBest so the first row is the recommended one.
    auto sorted = sources;
    std::sort(sorted.begin(), sorted.end(),
              [](const VideoSource& a, const VideoSource& b) {
                  if (a.kind != b.kind) {
                      if (a.kind == SourceKind::HTTP && b.kind != SourceKind::HTTP)
                          return true;
                      if (b.kind == SourceKind::HTTP && a.kind != SourceKind::HTTP)
                          return false;
                  }
                  if (a.bitrate != b.bitrate) return a.bitrate > b.bitrate;
                  return a.priority > b.priority;
              });

    const int32_t epid = episodeId_;
    int64_t resume = 0;
    for (size_t i = 0; i < sorted.size(); ++i) {
        const auto& s = sorted[i];
        // v22 §5.5: 88px elevated row + focus ring.
        auto* row = chrome::makeListRow(theme::kRowHeight);

        auto* badge = new brls::Label();
        badge->setText(i == 0 ? "推荐" : fmt::format("#{}", i + 1));
        badge->setFontSize(theme::kTypeCaption);
        badge->setTextColor(i == 0 ? theme::kAccentBright : theme::kDarkTextMuted);
        badge->setWidth(64);
        row->addView(badge);

        auto* name = new brls::Label();
        std::string label = s.label.empty() ? s.url : s.label;
        if (label.size() > 64) label = label.substr(0, 61) + "…";
        name->setText(label);
        name->setFontSize(theme::kTypeH3);
        name->setSingleLine(true);
        name->setGrow(1.0f);
        name->setTextColor(theme::kDarkTextPrimary);
        row->addView(name);

        const std::string url = s.url;
        row->registerClickAction([epid, url, resume](brls::View*) {
            pickLogf("RESOLVE: picker user-picked ep=%d url=%.80s", epid,
                     url.c_str());
            // videoSource carries the explicit URL — Player starts it
            // immediately (with download/load status). Local/CACHE paths
            // still go through openPlayer with the filesystem path.
            Intent::openPlayer(epid, "", url, resume);
            return true;
        });
        list_->addView(row);
    }

    // v22: autotour / field-test path — show the list briefly then
    // take the recommended source so online play can be verified
    // without input injection.
    if (autoPickFirst_) {
        if (sorted.empty()) {
            // Guard: never call this with empty, but if it happens the
            // message is "no sources", not "parse failed".
            setLoading(fmt::format(
                "自动选源失败：无可用播放源（ep={} sid={}）。\n"
                "不是解析崩溃——sources.json 无映射且在线源为空。",
                episodeId_, subjectId_));
            pickLogf("RESOLVE: picker autopick empty ep=%d", episodeId_);
            return;
        }
        const std::string url = sorted.front().url;
        const std::string label =
            sorted.front().label.empty() ? url : sorted.front().label;
        if (status_) {
            status_->setText(fmt::format("已选播放源，进入播放…  {}", label));
        }
        pickLogf("RESOLVE: picker autopick ep=%d label=%.60s", episodeId_,
                 label.c_str());
        const int32_t epid = episodeId_;
        Intent::openPlayer(epid, "", url, 0);
        return;
    }
}

}  // namespace aniswitch
