// SPDX-License-Identifier: AGPL-3.0

#include "ui/activity/local_video_activity.hpp"
#include "ui/theme.hpp"
#include "utils/activity_helper.hpp"
#include "utils/config_helper.hpp"
#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/scrolling_frame.hpp>
#include <fmt/format.h>
#include <algorithm>

#if defined(__SWITCH__) && defined(ANISWITCH_SWITCH_DEBUG)
extern "C" void aniswitchStartupLog(const char*);
#define LOCAL_TRACE(msg) aniswitchStartupLog(msg)
#else
#define LOCAL_TRACE(msg) do { (void)0; } while (0)
#endif

namespace aniswitch {

namespace {

std::string humanSize(int64_t bytes) {
    if (bytes <= 0) return "?";
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int idx = 0;
    double v = static_cast<double>(bytes);
    while (v >= 1024.0 && idx < 4) { v /= 1024.0; ++idx; }
    return fmt::format("{:.1f} {}", v, units[idx]);
}

}  // namespace

LocalVideoActivity::LocalVideoActivity() {
    LOCAL_TRACE("LOCAL: ctor");
}

void LocalVideoActivity::onContentAvailable() {
    LOCAL_TRACE("LOCAL: onContentAvailable begin");
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setPadding(20, 16, 20, 16);
#ifdef __SWITCH__
    root->setWidth(theme::kDesignWidth);
#endif

    auto* title = new brls::Label();
    title->setText("本地视频");
    title->setFontSize(theme::kTypeH2);
    title->setMarginBottom(8);
    root->addView(title);

    auto* hint = new brls::Label();
    hint->setText("sdmc:/switch/aniswitch/videos/");
    hint->setFontSize(theme::kTypeCaption);
    hint->setTextColor(nvgRGB(160, 160, 170));
    hint->setMarginBottom(12);
    root->addView(hint);

    statusLabel_ = new brls::Label();
    statusLabel_->setText("正在扫描...");
    statusLabel_->setFontSize(theme::kTypeCaption);
    statusLabel_->setMarginBottom(12);
    root->addView(statusLabel_);

    list_ = new brls::Box();
    list_->setAxis(brls::Axis::COLUMN);
    root->addView(list_);

    auto* scroll = new brls::ScrollingFrame();
    scroll->setContentView(root);
    scroll->setGrow(1.0f);
    setContentView(scroll);
    registerAction("返回", brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });
    LOCAL_TRACE("LOCAL: onContentAvailable end");

    // Scan off the constructor path so a bad FS walk cannot kill
    // activity push.
    std::weak_ptr<int> life = lifetime_;
    brls::delay(50, [this, life]() {
        if (life.expired()) return;
        LOCAL_TRACE("LOCAL: delayed scan begin");
        try {
            entries_ = LocalVideoScanner::scan();
        } catch (const std::exception& e) {
            brls::Logger::error("LocalVideo scan: {}", e.what());
            entries_.clear();
        } catch (...) {
            entries_.clear();
        }
        LOCAL_TRACE("LOCAL: delayed scan done");
        renderList();
    });
}

void LocalVideoActivity::renderList() {
    LOCAL_TRACE("LOCAL: renderList");
    if (!list_) return;
    list_->clearViews();
    if (statusLabel_) {
        if (entries_.empty()) {
            statusLabel_->setText(
                "没有找到视频\n"
                "请把 .mp4/.mkv 拷贝到:\n"
                "  sdmc:/switch/aniswitch/videos/\n"
                "（也支持该目录下的子文件夹）");
            statusLabel_->setSingleLine(false);
            statusLabel_->setFontSize(theme::kTypeBody);
        } else {
            statusLabel_->setText(fmt::format("共 {} 个文件 · sdmc:/switch/aniswitch/videos/",
                                              entries_.size()));
        }
    }
    if (entries_.empty()) return;

    int shown = 0;
    for (const auto& e : entries_) {
        if (shown >= 30) break;
        auto* row = new brls::Box();
        row->setAxis(brls::Axis::ROW);
        row->setPadding(20, 12, 20, 12);
        row->setMarginBottom(8);
        row->setBackground(brls::ViewBackground::SHAPE_COLOR);
        row->setBackgroundColor(nvgRGBA(28, 26, 36, 220));
        row->setCornerRadius(8);
        row->setHeight(theme::kRowHeight);
        row->setFocusable(true);
        theme::applyFocusStyle(row, 8.f);
#ifdef __SWITCH__
        row->setWidth(980);
#endif
        auto* label = new brls::Label();
        label->setText(fmt::format("{}  ({})", e.filename, humanSize(e.sizeBytes)));
        label->setFontSize(theme::kTypeH3);
        label->setSingleLine(true);
        label->setGrow(1.0f);
        row->addView(label);
        const std::string path = e.path;
        row->registerClickAction([path](brls::View*) {
            Intent::openPlayer(/*episodeId=*/-1, "", path, 0);
            return true;
        });
        list_->addView(row);
        ++shown;
    }
    LOCAL_TRACE("LOCAL: renderList done");
}

}  // namespace aniswitch
