// SPDX-License-Identifier: AGPL-3.0
#include "ui/fragment/home_bangumi.hpp"
#include "ui/poster_card.hpp"
#include "ui/presenter/home_presenter.hpp"
#include "ui/status_view.hpp"
#include "ui/theme.hpp"
#include "ui/demo_data.hpp"
#include "net/http.hpp"
#include "utils/activity_helper.hpp"
#include "utils/image_loader.hpp"
#include <borealis/core/thread.hpp>
#include <borealis/views/scrolling_frame.hpp>
#include <chrono>
#include <ctime>
#include <fmt/format.h>

namespace aniswitch {

namespace {
// Bangumi calendar week order: Mon=0, Tue=1, ..., Sun=6.
// C tm_wday: Sun=0, Mon=1, ..., Sat=6.
// Map: Bangumi = (tm_wday + 6) % 7.
int todayBangumiWeekday() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return (tm.tm_wday + 6) % 7;
}

const char* weekdayCN(int bangumiWeekday) {
    static const char* kNames[7] = {
        "周一", "周二", "周三", "周四", "周五", "周六", "周日"
    };
    if (bangumiWeekday < 0 || bangumiWeekday >= 7) return "";
    return kNames[bangumiWeekday];
}
}  // namespace

HomeBangumiFragment::HomeBangumiFragment() {
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setPadding(0, 0, theme::kHudHeight / 2, 0);
    root->setBackgroundColor(theme::kChromeBg);
#ifdef __SWITCH__
    root->setWidth(theme::kContentWidth);
#endif

    auto* header = new brls::Box();
    header->setAxis(brls::Axis::COLUMN);
    header->setPadding(0, 0, 0, 0);
    auto* title = new brls::Label();
    title->setText("每日放送");
    title->setFontSize(theme::kTypeH2);
    title->setTextColor(theme::kDarkTextPrimary);
    title->setMarginBottom(6);
    header->addView(title);
    auto* hint = new brls::Label();
    hint->setText("本周新番时间表（今日高亮）");
    hint->setFontSize(theme::kTypeCaption);
    hint->setTextColor(theme::kDarkTextMuted);
    hint->setMarginBottom(18);
    header->addView(hint);
    root->addView(header);

    status_ = new StatusView();
    status_->setLoading("加载每日放送...");
    status_->setGrow(1);
    root->addView(status_);

    list_ = new brls::Box();
    list_->setAxis(brls::Axis::COLUMN);
    list_->setVisibility(brls::Visibility::GONE);
#ifdef __SWITCH__
    // startup.13: explicit width — percentage cells inside this list
    // collapsed when the parent auto-sized to the day-header label.
    list_->setWidth(theme::kContentWidth);
#endif
    auto* scroll = new brls::ScrollingFrame();
    scroll->setContentView(list_);
    scroll->setGrow(1);
#ifdef __SWITCH__
    scroll->setWidth(theme::kDesignWidth);
#endif
    root->addView(scroll);

    addView(root);

    presenter_.onCalendar.subscribe([this](std::vector<CalendarItem> v) {
        onCalendar(std::move(v));
    });
    refresh();

    // v17.6.3: timeout fallback.  v17.6.2 wired a setError()
    // with a "重试" button whose click handler called
    // refresh() — and refresh() goes right back to the same
    // cpr::Get that just hung, so the click just rebooted
    // the hang.  v17.6.3 drops the retry button and uses
    // setEmpty() (a static 'o' glyph + detail text) so the
    // user sees the message and can move on to other tabs.
    armTimeout();
}

HomeBangumiFragment::~HomeBangumiFragment() {
    if (timeoutToken_) {
        brls::cancelDelay(timeoutToken_);
        timeoutToken_ = 0;
    }
}

void HomeBangumiFragment::armTimeout() {
    if (timeoutToken_) brls::cancelDelay(timeoutToken_);
    timeoutToken_ = brls::delay(8000, [this]() {
        if (!loading_) return;
        status_->setEmpty("加载超时\n" + HTTP::proxyHint());
    });
}

void HomeBangumiFragment::refresh() {
    loading_ = true;
    status_->setLoading("加载每日放送...");
    list_->setVisibility(brls::Visibility::GONE);
    armTimeout();
    presenter_.refresh();
}

void HomeBangumiFragment::onCalendar(std::vector<CalendarItem> calendar) {
    loading_ = false;
    list_->clearViews();
    if (calendar.empty()) {
        // v22 compose-next: empty schedule → demo week.
        auto* banner = new brls::Label();
        banner->setText(demo::kBanner);
        banner->setFontSize(theme::kTypeCaption);
        banner->setTextColor(theme::kDarkTextMuted);
        banner->setMarginBottom(8);
        list_->addView(banner);
        calendar = demo::calendar();
    }
    status_->setVisibility(brls::Visibility::GONE);
    list_->setVisibility(brls::Visibility::VISIBLE);
    const int today = todayBangumiWeekday();
    int lastWeekday = -999;
    std::vector<brls::Box*> dayCells;
    auto flushDay = [this, &dayCells]() {
        // Fixed pixel cells (startup.13): 49% of an unsized parent
        // produced ~200px clipped cards on the daily schedule tab.
        const float cellW = (theme::kContentWidth - 12) / 2.0f;
        for (size_t i = 0; i < dayCells.size(); i += 2) {
            auto* pair = new brls::Box();
            pair->setAxis(brls::Axis::ROW);
#ifdef __SWITCH__
            pair->setWidth(theme::kContentWidth);
#endif
            auto* left = dayCells[i];
            left->setWidth(cellW);
            left->setMarginRight(12);
            pair->addView(left);
            if (i + 1 < dayCells.size()) {
                auto* right = dayCells[i + 1];
                right->setMarginRight(0);
                right->setWidth(cellW);
                pair->addView(right);
            }
            list_->addView(pair);
        }
        dayCells.clear();
    };
    for (const auto& day : calendar) {
        const int bangumiWeekday = (day.weekday + 6) % 7;
        if (bangumiWeekday != lastWeekday) {
            flushDay();
            lastWeekday = bangumiWeekday;
            auto* dayHeader = new brls::Label();
            dayHeader->setText(weekdayCN(bangumiWeekday));
            dayHeader->setFontSize(theme::kTypeH3);
            dayHeader->setSingleLine(true);
            dayHeader->setMarginTop(10);
            dayHeader->setMarginBottom(6);
            if (bangumiWeekday == today) {
                dayHeader->setTextColor(nvgRGB(167, 139, 250));
            }
            list_->addView(dayHeader);
        }
        const Subject& s = day.subject;
        if (s.id == 0 && s.name.empty() && s.nameCN.empty()) continue;
        auto* row = new brls::Box();
        row->setAxis(brls::Axis::ROW);
        row->setPadding(12, 8, 12, 8);
        row->setMarginBottom(8);
        row->setBackground(brls::ViewBackground::SHAPE_COLOR);
        row->setBackgroundColor(theme::kDarkCardRowBg);
        row->setCornerRadius(8);
        row->setHeight(theme::kRowHeight);
        row->setFocusable(true);
        theme::applyFocusStyle(row, 8.f);
        const std::string url = pickImageUrl(s.images);
        if (theme::kCoverEnabled && !url.empty()) {
            auto* thumb = new brls::Image();
            thumb->setWidth(48);
            thumb->setHeight(72);
            thumb->setScalingType(brls::ImageScalingType::FILL);
            thumb->setCornerRadius(4);
            thumb->setMarginRight(12);
            row->addView(thumb);
            ImageLoader::instance().load(
                url,
                [thumb](const std::string& path) {
                    if (path.empty()) return;
                    brls::sync([thumb, path]() {
                        thumb->setImageFromFile(path);
                    });
                },
                fmt::format("day-{}", s.id));
        }
        auto* label = new brls::Label();
        std::string t = !s.nameCN.empty() ? s.nameCN : s.name;
        if (t.empty()) t = "subject " + std::to_string(s.id);
        label->setText(t);
        label->setFontSize(theme::kTypeCard);
        label->setSingleLine(true);
        label->setGrow(1.0f);
        row->addView(label);
        const int32_t sid = s.id;
        row->registerClickAction([sid](brls::View*) {
            Intent::openSubject(sid);
            return true;
        });
        dayCells.push_back(row);
    }
    flushDay();
}

}  // namespace aniswitch
