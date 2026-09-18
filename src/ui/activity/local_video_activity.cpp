// SPDX-License-Identifier: AGPL-3.0
//
// v22 chrome + anime-name grouping:
//   * chrome::makePageRoot / title / path caption / status
//   * entries grouped by LocalVideoEntry::series
//   * section chrome::makeSection(series + " · N") + chrome::makeListRow
//   * scroll shell + B + HUD via chrome::attachScrollShell/registerBack/finish
//   * theme tokens only — no raw nvgRGB, no 16px side pad, no row width 980
// openPlayer(-1, "", path, 0) is unchanged.

#include "ui/activity/local_video_activity.hpp"
#include "ui/theme.hpp"
#include "ui/ui_chrome.hpp"
#include "utils/activity_helper.hpp"
#include <borealis/core/logger.hpp>
#include <borealis/core/thread.hpp>
#include <fmt/format.h>
#include <map>
#include <string>
#include <vector>

#if defined(__SWITCH__) && defined(ANISWITCH_SWITCH_DEBUG)
extern "C" void aniswitchStartupLog(const char*);
#define LOCAL_TRACE(msg) aniswitchStartupLog(msg)
#else
#define LOCAL_TRACE(msg) do { (void)0; } while (0)
#endif

namespace aniswitch {

namespace {

// Display caps — section headers still show the REAL group count.
constexpr int kMaxFiles  = 40;
constexpr int kMaxGroups = 20;

std::string humanSize(int64_t bytes) {
    if (bytes <= 0) return "?";
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int idx = 0;
    double v = static_cast<double>(bytes);
    while (v >= 1024.0 && idx < 4) { v /= 1024.0; ++idx; }
    return fmt::format("{:.1f} {}", v, units[idx]);
}

struct SeriesGroup {
    std::string series;
    std::vector<const LocalVideoEntry*> items;
};

// Stable grouping: first appearance order (entries_ is already mtime desc).
std::vector<SeriesGroup> groupBySeries(const std::vector<LocalVideoEntry>& entries) {
    std::vector<SeriesGroup> groups;
    std::map<std::string, size_t> index;
    for (const auto& e : entries) {
        const std::string key = e.series.empty() ? std::string("未分类") : e.series;
        auto it = index.find(key);
        if (it == index.end()) {
            index[key] = groups.size();
            groups.push_back(SeriesGroup{key, {&e}});
        } else {
            groups[it->second].items.push_back(&e);
        }
    }
    return groups;
}

}  // namespace

LocalVideoActivity::LocalVideoActivity() {
    LOCAL_TRACE("LOCAL: ctor");
}

void LocalVideoActivity::onContentAvailable() {
    LOCAL_TRACE("LOCAL: onContentAvailable begin");
    auto* root = chrome::makePageRoot();  // kChromeBg + safe margins

    root->addView(chrome::makeTitle("本地视频", theme::kTypeH2, 8));
    root->addView(chrome::makeCaption("sdmc:/switch/aniswitch/videos/", 12));

    statusLabel_ = new brls::Label();
    statusLabel_->setText("正在扫描...");
    statusLabel_->setFontSize(theme::kTypeCaption);
    statusLabel_->setTextColor(theme::kDarkTextMuted);
    statusLabel_->setSingleLine(false);
    statusLabel_->setMarginBottom(12);
    root->addView(statusLabel_);

    list_ = new brls::Box();
    list_->setAxis(brls::Axis::COLUMN);
    root->addView(list_);

    auto* shell = chrome::attachScrollShell(this, root);
    if (shell) shell->setBackgroundColor(theme::kChromeBg);
    chrome::registerBack(this);
    // HUD chips come from the live ActionMap — after registerBack.
    chrome::finish(this, shell);
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
            // Multi-line empty hint stays; colors via theme tokens.
            statusLabel_->setText(
                "没有找到视频\n"
                "请把 .mp4/.mkv 拷贝到:\n"
                "  sdmc:/switch/aniswitch/videos/\n"
                "（也支持该目录下的子文件夹；子文件夹名会作为系列分组）");
            statusLabel_->setSingleLine(false);
            statusLabel_->setFontSize(theme::kTypeBody);
            statusLabel_->setTextColor(theme::kDarkTextSecondary);
            return;
        }
    }
    if (entries_.empty()) return;

    const auto groups = groupBySeries(entries_);
    if (statusLabel_) {
        statusLabel_->setText(fmt::format(
            "共 {} 个文件 · {} 个系列 · sdmc:/switch/aniswitch/videos/",
            entries_.size(), groups.size()));
        statusLabel_->setSingleLine(true);
        statusLabel_->setFontSize(theme::kTypeCaption);
        statusLabel_->setTextColor(theme::kDarkTextMuted);
    }

    int filesShown  = 0;
    int groupsShown = 0;
    for (const auto& g : groups) {
        if (groupsShown >= kMaxGroups) break;
        if (filesShown >= kMaxFiles) break;

        // Real group count in the header, even when rows are capped.
        list_->addView(chrome::makeSection(
            fmt::format("{} · {}", g.series, g.items.size())));

        for (const auto* e : g.items) {
            if (filesShown >= kMaxFiles) break;

            auto* row = chrome::makeListRow(theme::kRowHeight);

            // filename kTypeH3 + size kTypeCaption; optional source.
            std::string subtitle = humanSize(e->sizeBytes);
            if (!e->source.empty()) subtitle += "  ·  " + e->source;
            row->addView(chrome::makeTextColumn(e->filename, subtitle));

            const std::string path = e->path;  // full path, subfolders intact
            row->registerClickAction([path](brls::View*) {
                Intent::openPlayer(/*episodeId=*/-1, "", path, 0);
                return true;
            });
            list_->addView(row);
            ++filesShown;
        }
        ++groupsShown;
    }
    LOCAL_TRACE("LOCAL: renderList done");
}

}  // namespace aniswitch
