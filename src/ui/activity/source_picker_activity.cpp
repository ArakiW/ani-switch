// SPDX-License-Identifier: AGPL-3.0
#include "ui/activity/source_picker_activity.hpp"
#include "core/episode_resolver.hpp"
#include "ui/hud.hpp"
#include "ui/theme.hpp"
#include "ui/demo_data.hpp"
#include "utils/activity_helper.hpp"
#include <borealis/core/logger.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/scrolling_frame.hpp>
#include <algorithm>
#include <fmt/format.h>
#include <string>

#if defined(__SWITCH__)
extern "C" void aniswitchStartupLog(const char* message);
#endif

namespace aniswitch {

SourcePickerActivity::SourcePickerActivity(int32_t episodeId,
                                           int32_t subjectId,
                                           const std::string& title,
                                           bool autoPickFirst)
    : episodeId_(episodeId), subjectId_(subjectId), title_(title),
      autoPickFirst_(autoPickFirst) {}

void SourcePickerActivity::onContentAvailable() {
    auto* scroll = new brls::ScrollingFrame();

    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setPadding(20, theme::kSafeMarginX, 20, theme::kSafeMarginX);
#ifdef __SWITCH__
    root->setWidth(theme::kDesignWidth);
#endif
    root->setBackgroundColor(theme::kChromeBg);

    auto* title = new brls::Label();
    title->setText(title_.empty()
                       ? fmt::format("选择播放源 · EP{}", episodeId_)
                       : fmt::format("选择播放源 · {}", title_));
    title->setFontSize(theme::kTypeH2);
    title->setTextColor(theme::kDarkTextPrimary);
    title->setMarginBottom(8);
    root->addView(title);

    status_ = new brls::Label();
    status_->setText("正在解析播放源…");
    status_->setFontSize(theme::kTypeCaption);
    status_->setTextColor(theme::kDarkTextMuted);
    status_->setMarginBottom(16);
    root->addView(status_);

    list_ = new brls::Box();
    list_->setAxis(brls::Axis::COLUMN);
    root->addView(list_);

    scroll->setContentView(root);
    auto* shell = setContentViewWithHudShell(this, scroll);
    registerAction("返回", brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });
    appendHud(this, shell);

    // v22: demo / negative ids — skip network, offer local sample.
    if (episodeId_ < 0) {
        VideoSource local;
        local.kind = SourceKind::CACHE;
        local.url = "sdmc:/switch/aniswitch/videos/test-local.mp4";
        local.label = "本地测试视频（演示）";
        local.priority = 0;
        std::vector<VideoSource> demoSources{local};
        renderSources(demoSources);
        return;
    }

    setLoading("正在解析播放源…（在线检索 / 本地清单）");
#if defined(__SWITCH__)
    aniswitchStartupLog("PICKER: resolve begin");
#endif
    std::weak_ptr<int> life = lifetime_;
    EpisodeResolver::resolve(subjectId_, episodeId_,
        [this, life](ResolvedEpisode result) {
            brls::sync([this, life, result] {
                if (life.expired()) return;
#if defined(__SWITCH__)
                {
                    char b[100];
                    snprintf(b, sizeof(b), "PICKER: got %zu source(s)",
                             result.sources.size());
                    aniswitchStartupLog(b);
                }
#endif
                if (result.sources.empty()) {
                    setLoading("没有找到在线播放源。\n"
                               "可检查 sources.json / 网络，或返回后从本地视频播放。");
                    return;
                }
                renderSources(result.sources);
            });
        },
        [this, life](const std::string& msg, int) {
            brls::sync([this, life, msg] {
                if (life.expired()) return;
                setLoading("解析失败: " + msg);
            });
        });
}

void SourcePickerActivity::setLoading(const std::string& msg) {
    if (status_) status_->setText(msg);
}

void SourcePickerActivity::renderSources(std::vector<VideoSource> sources) {
    if (!list_) return;
    list_->clearViews();
    if (status_) {
        status_->setText(fmt::format("共 {} 个播放源，请选择", sources.size()));
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
        auto* row = new brls::Box();
        row->setFocusable(true);
        row->setAxis(brls::Axis::ROW);
        row->setHeight(theme::kRowHeight);
        row->setPadding(16, 12, 16, 12);
        row->setBackground(brls::ViewBackground::SHAPE_COLOR);
        row->setBackgroundColor(theme::kDarkCardRowBg);
        row->setCornerRadius(8);
        row->setMarginBottom(8);
        theme::applyFocusStyle(row, 8.f);

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
#if defined(__SWITCH__)
            aniswitchStartupLog("PICKER: user picked source → player");
#endif
            // videoSource carries the explicit URL — Player starts it
            // immediately (with download/load status).
            Intent::openPlayer(epid, "", url, resume);
            return true;
        });
        list_->addView(row);
    }

    // v22: autotour / field-test path — show the list briefly then
    // take the recommended source so online play can be verified
    // without input injection.
    if (autoPickFirst_ && !sorted.empty()) {
        const std::string url = sorted.front().url;
        const std::string label =
            sorted.front().label.empty() ? url : sorted.front().label;
        if (status_) {
            status_->setText("已选推荐源，加载中…  " + label);
        }
#if defined(__SWITCH__)
        aniswitchStartupLog("PICKER: autopick first source");
#endif
        const int32_t epid = episodeId_;
        std::weak_ptr<int> life = lifetime_;
        brls::delay(1200, [life, epid, url]() {
            if (life.expired()) return;
            Intent::openPlayer(epid, "", url, 0);
        });
    }
}

}  // namespace aniswitch
