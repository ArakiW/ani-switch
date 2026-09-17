// SPDX-License-Identifier: AGPL-3.0
//
// v17.5: history view v2.
//   • summary line at the top (count + total watch time)
//   • "清空所有" button with confirm dialog
//   • entries grouped by "今天 / 本周 / 更早" sections
//   • per-row "✕" delete button (calls SQLiteStore::deleteHistory)
//   • per-row relative timestamp ("刚刚" / "3 分钟前" / "2 小时前" /
//     "5 天前")
//
// Pure-local: no network, no DB schema change.  Time is sourced
// from time() once per render and the buckets are recomputed
// against that, so when the user reaches the page later the
// "今天" / "本周" cutoff slides forward naturally.

#include "ui/activity/history_activity.hpp"
#include "ui/theme.hpp"
#include "ui/hud.hpp"
#include "utils/activity_helper.hpp"
#include "utils/string_helper.hpp"
#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>
#include <fmt/format.h>
#include <borealis/views/dialog.hpp>
#include <ctime>
#include <vector>

namespace aniswitch {

namespace {

constexpr int64_t kSecondsPerDay   = 24 * 60 * 60;
constexpr int64_t kSecondsPerWeek  = 7 * kSecondsPerDay;

enum class HistoryBucket { Today, ThisWeek, Older };

HistoryBucket bucketFor(int64_t watchedAtUnix, int64_t nowUnix) {
    if (watchedAtUnix <= 0) return HistoryBucket::Older;
    int64_t ageSec = nowUnix - watchedAtUnix;
    if (ageSec < 0)             return HistoryBucket::Today;   // future timestamp → now
    if (ageSec < kSecondsPerDay)  return HistoryBucket::Today;
    if (ageSec < kSecondsPerWeek) return HistoryBucket::ThisWeek;
    return HistoryBucket::Older;
}

const char* bucketLabel(HistoryBucket b) {
    switch (b) {
        case HistoryBucket::Today:    return "今天";
        case HistoryBucket::ThisWeek: return "本周";
        case HistoryBucket::Older:    return "更早";
    }
    return "";
}

std::string relativeAge(int64_t watchedAtUnix, int64_t nowUnix) {
    if (watchedAtUnix <= 0) return "—";
    int64_t age = nowUnix - watchedAtUnix;
    if (age < 60)            return "刚刚";
    if (age < 3600)          return "几分钟前";
    if (age < kSecondsPerDay) {
        return fmt::format("{} 小时前", age / 3600);
    }
    if (age < kSecondsPerWeek) {
        return fmt::format("{} 天前", age / kSecondsPerDay);
    }
    return fmt::format("{} 周前", age / kSecondsPerWeek);
}

std::string formatTotalDuration(int64_t totalMs) {
    int64_t totalSec = totalMs / 1000;
    int64_t hours   = totalSec / 3600;
    int64_t minutes = (totalSec % 3600) / 60;
    if (hours > 0) return fmt::format("{} 小时 {} 分钟", hours, minutes);
    return fmt::format("{} 分钟", minutes);
}

}  // namespace

HistoryActivity::HistoryActivity() = default;

void HistoryActivity::onContentAvailable() {
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setPadding(20);

    auto* title = new brls::Label();
    title->setText("观看历史");
    title->setFontSize(theme::kTypeH2);
    title->setMarginBottom(6);
    root->addView(title);

    // Live summary, updated by render().
    summary_ = new brls::Label();
    summary_->setFontSize(theme::kTypeCaption);
    summary_->setTextColor(aniswitch::theme::kDarkTextSecondary);
    summary_->setMarginBottom(8);
    root->addView(summary_);

    clearBtn_ = new brls::Button();
    clearBtn_->setText("清空所有");
    clearBtn_->setHeight(theme::kButtonHeight);
    theme::applyFocusStyle(clearBtn_);
    clearBtn_->setMarginBottom(12);
    clearBtn_->registerClickAction([this](brls::View*) {
        // v17.5: use the single-arg Dialog constructor and add
        // both buttons via addButton() — the 4-arg constructor
        // doesn't exist in borealis 5f08b286.
        auto* dlg = new brls::Dialog("确定要清空所有观看历史?  此操作无法恢复。");
        dlg->setCancelable(true);
        dlg->addButton("取消", []() {});
        dlg->addButton("清空", [this]() {
            SQLiteStore::instance().clearAllHistory();
            presenter_.refresh();
        });
        dlg->open();
        return true;
    });
    root->addView(clearBtn_);

    // List lives in a ScrollingFrame so 100+ entries don't push
    // the title off the screen.
    list_ = new brls::Box();
    list_->setAxis(brls::Axis::COLUMN);
    root->addView(list_);

    auto* scroll = new brls::ScrollingFrame();
    scroll->setContentView(root);
    auto* shell = setContentViewWithHudShell(this, scroll);
    registerAction("返回", brls::BUTTON_B, [](brls::View*) { brls::Application::popActivity(); return true; });
    registerAction("清空所有", brls::BUTTON_X, [this](brls::View*) {
        auto* dlg = new brls::Dialog("确定要清空所有观看历史?  此操作无法恢复。");
        dlg->setCancelable(true);
        dlg->addButton("取消", [] {});
        dlg->addButton("清空", [this]() {
            SQLiteStore::instance().clearAllHistory();
            presenter_.refresh();
        });
        dlg->open();
        return true;
    });
    appendHud(this, shell);

    presenter_.onHistory.subscribe([this](std::vector<SQLiteStore::HistoryEntry> v) {
        render(v);
    });
    presenter_.refresh();
}

void HistoryActivity::render(const std::vector<SQLiteStore::HistoryEntry>& v) {
    if (!list_) return;
    list_->clearViews();

    if (summary_) {
        if (v.empty()) {
            summary_->setText("还没有任何观看记录");
        } else {
            int64_t totalMs = 0;
            for (const auto& e : v) totalMs += e.durationMs;
            summary_->setText(fmt::format("共 {} 条,累计 {}",
                                          v.size(),
                                          formatTotalDuration(totalMs)));
        }
    }

    if (v.empty()) {
        auto* empty = new brls::Label();
        empty->setText("(无)");
        empty->setFontSize(theme::kTypeBody);
        list_->addView(empty);
        return;
    }

    int64_t now = static_cast<int64_t>(time(nullptr));

    // Group entries by bucket, preserving the order we received
    // them in (getHistory already sorts by watchedAt desc).
    std::vector<std::vector<const SQLiteStore::HistoryEntry*>> buckets(3);
    for (const auto& e : v) {
        buckets[static_cast<int>(bucketFor(e.watchedAt, now))].push_back(&e);
    }
    for (int i = 0; i < 3; ++i) {
        if (buckets[i].empty()) continue;
        auto* header = new brls::Box();
        header->setAxis(brls::Axis::ROW);
        header->setPadding(0, 0, 0, 6);
        header->setMarginTop(8);
        header->setMarginBottom(4);
        auto* bar = new brls::Rectangle();
        bar->setSize(brls::Size(20, 2));
        bar->setColor(aniswitch::theme::kAccent);
        header->addView(bar);
        auto* lbl = new brls::Label();
        lbl->setText(fmt::format("  {} ({} 条)",
                                 bucketLabel(static_cast<HistoryBucket>(i)),
                                 buckets[i].size()));
        lbl->setFontSize(theme::kTypeH3);
        lbl->setTextColor(aniswitch::theme::kAccent);
        header->addView(lbl);
        list_->addView(header);

        for (const auto* ep : buckets[i]) {
            const auto& e = *ep;
            auto* row = new brls::Box();
            row->setFocusable(true);
            row->setHeight(theme::kRowHeight);
            row->setPadding(12, 12, 12, 12);
            theme::applyFocusStyle(row, 8.f);
            row->setAxis(brls::Axis::ROW);
            row->setAlignItems(brls::AlignItems::CENTER);
            row->setMarginBottom(6);

            // v22 §5.5: 56×84 thumbnail (cover-gated; color block fallback).
            auto* thumb = new brls::Box();
            thumb->setWidth(56);
            thumb->setHeight(84);
            thumb->setCornerRadius(6);
            thumb->setBackground(brls::ViewBackground::SHAPE_COLOR);
            thumb->setBackgroundColor(nvgRGB(45, 40, 60));
            thumb->setMarginRight(12);
            row->addView(thumb);

            auto* bodyCol = new brls::Box();
            bodyCol->setAxis(brls::Axis::COLUMN);
            bodyCol->setGrow(1.0f);

            auto* title = new brls::Label();
            title->setText(fmt::format("{} - {}",
                                       e.subjectName, e.episodeName));
            title->setFontSize(theme::kTypeH3);
            title->setSingleLine(true);
            bodyCol->addView(title);

            auto* meta = new brls::Label();
            std::string metaText = relativeAge(e.watchedAt, now);
            if (e.durationMs > 0) {
                metaText += fmt::format("  •  {}/{}",
                                        format_duration(e.positionMs / 1000.0),
                                        format_duration(e.durationMs / 1000.0));
            }
            meta->setText(metaText);
            meta->setFontSize(theme::kTypeCaption);
            meta->setTextColor(aniswitch::theme::kDarkTextMuted);
            bodyCol->addView(meta);
            row->addView(bodyCol);

            // Click on the row body resumes playback.
            int32_t eid = e.episodeId;
            int64_t pos = e.positionMs;
            auto path = e.episodeName;
            row->registerClickAction([eid, pos, path](brls::View*) {
                Intent::openPlayer(eid, "dandanplay", path, pos);
                return true;
            });

            // v22 §5.5: trailing 48px key-hint zone holds delete.
            auto* delBtn = new brls::Button();
            delBtn->setText("✕");
            delBtn->setWidth(48);
            delBtn->setHeight(48);
            delBtn->setMarginLeft(8);
            theme::applyFocusStyle(delBtn, 8.f);
            int32_t deleteId = e.episodeId;
            delBtn->registerClickAction([this, deleteId](brls::View*) {
                // v22 §7.1: destructive confirm, default focus on 取消.
                auto* dlg = new brls::Dialog("删除这条观看记录？");
                dlg->setCancelable(true);
                dlg->addButton("取消", [] {});
                dlg->addButton("删除", [this, deleteId]() {
                    SQLiteStore::instance().deleteHistory(deleteId);
                    presenter_.refresh();
                });
                dlg->open();
                return true;
            });
            row->addView(delBtn);

            list_->addView(row);
        }
    }
}

}  // namespace aniswitch
