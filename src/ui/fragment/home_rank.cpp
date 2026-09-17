// SPDX-License-Identifier: AGPL-3.0
#include "ui/fragment/home_rank.hpp"
#include "ui/poster_card.hpp"
#include "ui/presenter/home_presenter.hpp"
#include "ui/status_view.hpp"
#include "ui/theme.hpp"
#include "net/http.hpp"
#include "utils/activity_helper.hpp"
#include "utils/image_loader.hpp"
#include <borealis/core/thread.hpp>
#include <borealis/views/scrolling_frame.hpp>
#include <borealis/views/h_scrolling_frame.hpp>
#include <fmt/format.h>
#if defined(__SWITCH__)
extern "C" void aniswitchStartupLog(const char* message);
#else
static inline void aniswitchStartupLog(const char* m) { if (m) brls::Logger::info("{}", m); }
#endif

namespace aniswitch {

HomeRankFragment::HomeRankFragment() {
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setPadding(0, 0, theme::kHudHeight / 2, 0);
    root->setBackgroundColor(theme::kChromeBg);

    auto* title = new brls::Label();
    title->setText("本周热门");
    title->setFontSize(theme::kTypeH2);
    title->setTextColor(theme::kDarkTextPrimary);
    title->setSingleLine(true);
    title->setMarginBottom(6);
    root->addView(title);
    auto* hint = new brls::Label();
    hint->setText("Ani · /v1/trends · L/R 切换栏目");
    hint->setFontSize(theme::kTypeCaption);
    hint->setTextColor(theme::kDarkTextMuted);
    hint->setSingleLine(true);
    hint->setMarginBottom(12);
    root->addView(hint);

    status_ = new StatusView();
    status_->setLoading("加载热门...");
    status_->setGrow(1);
    root->addView(status_);

    // v22 P1: horizontal poster rail via borealis HScrollingFrame.
    // Content is a ROW box of PosterCards; padding lives on the content
    // view (HScrollingFrame rejects setPadding on itself).
    list_ = new brls::Box();
    list_->setAxis(brls::Axis::ROW);
    list_->setPadding(8, 8, 12, 0);  // room for focus ring
    list_->setVisibility(brls::Visibility::GONE);

    auto* rail = new brls::HScrollingFrame();
    rail->setContentView(list_);
    // Poster 228 + bottom plate ~72 + focus pad; keep tight so
    // 「全部条目」sits close under the rail (user: gap too large).
    rail->setHeight(theme::kPosterH + 28);
    rail->setWidthPercentage(100.0f);
    rail->setGrow(0);
    rail->setScrollingIndicatorVisible(false);
    root->addView(rail);

    // Secondary 2-column dense grid under the rail.
    auto* moreLabel = new brls::Label();
    moreLabel->setText("全部条目");
    moreLabel->setFontSize(theme::kTypeH3);
    moreLabel->setSingleLine(true);
    moreLabel->setMarginTop(4);
    moreLabel->setMarginBottom(6);
    root->addView(moreLabel);

    listRows_ = new brls::Box();
    listRows_->setAxis(brls::Axis::COLUMN);
    listRows_->setVisibility(brls::Visibility::GONE);
    auto* scroll = new brls::ScrollingFrame();
    scroll->setContentView(listRows_);
    scroll->setGrow(1);
    root->addView(scroll);

    addView(root);

    presenter_.onTrending.subscribe([this](std::vector<SearchSubject> v) {
        onTrending(std::move(v));
    });
    refresh();
    armTimeout();
}

HomeRankFragment::~HomeRankFragment() {
    if (timeoutToken_) {
        brls::cancelDelay(timeoutToken_);
        timeoutToken_ = 0;
    }
    if (coverToken_) {
        brls::cancelDelay(coverToken_);
        coverToken_ = 0;
    }
}

void HomeRankFragment::armTimeout() {
    if (timeoutToken_) brls::cancelDelay(timeoutToken_);
    timeoutToken_ = brls::delay(8000, [this]() {
        if (!loading_) return;
        status_->setEmpty("加载超时\n" + HTTP::proxyHint());
    });
}

void HomeRankFragment::refresh() {
    loading_ = true;
    covers_.clear();
    status_->setLoading("加载热门...");
    list_->setVisibility(brls::Visibility::GONE);
    listRows_->setVisibility(brls::Visibility::GONE);
    armTimeout();
    presenter_.fetchTrending();
}

void HomeRankFragment::onTrending(std::vector<SearchSubject> subjects) {
    aniswitchStartupLog("HOME: onTrending enter");
    loading_ = false;
    covers_.clear();
    list_->clearViews();
    listRows_->clearViews();
    if (subjects.empty()) {
        status_->setEmpty("暂无热门番剧\n" + HTTP::proxyHint());
        return;
    }
    if (subjects.size() > 20) subjects.resize(20);
    aniswitchStartupLog("HOME: onTrending render");

    status_->setVisibility(brls::Visibility::GONE);
    list_->setVisibility(brls::Visibility::VISIBLE);
    listRows_->setVisibility(brls::Visibility::VISIBLE);
    try {
        for (const auto& s : subjects) {
            const std::string t = !s.nameCN.empty() ? s.nameCN : s.name;
            const int32_t id = s.id;
            auto* card = makePosterCard(
                id, t.empty() ? ("subject " + std::to_string(id)) : t,
                static_cast<float>(s.score), pickImageUrl(s.images),
                theme::kPosterW, theme::kPosterH,
                [id]() { Intent::openSubject(id); });
            list_->addView(card);
        }
        // 2-column compact grid (user: single column showed too little).
        auto makeCell = [](const SearchSubject& s) -> brls::Box* {
            const int32_t id = s.id;
            std::string t = !s.nameCN.empty() ? s.nameCN : s.name;
            if (t.empty()) t = "subject " + std::to_string(id);
            if (s.score > 0) t += fmt::format("  {:.1f}", s.score);
            auto* row = new brls::Box();
            row->setAxis(brls::Axis::ROW);
            row->setPadding(12, 8, 12, 8);
            row->setMarginRight(12);
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
                    fmt::format("rankcell-{}", id));
            }
            auto* label = new brls::Label();
            label->setText(t);
            label->setFontSize(theme::kTypeCard);
            label->setSingleLine(true);
            label->setGrow(1.0f);
            label->setTextColor(theme::kDarkTextPrimary);
            row->addView(label);
            row->registerClickAction([id](brls::View*) {
                Intent::openSubject(id);
                return true;
            });
            return row;
        };
        for (size_t i = 0; i < subjects.size(); i += 2) {
            auto* pair = new brls::Box();
            pair->setAxis(brls::Axis::ROW);
            pair->setMarginBottom(0);
#ifdef __SWITCH__
            pair->setWidth(theme::kContentWidth);
#endif
            auto* left = makeCell(subjects[i]);
            left->setWidthPercentage(49.0f);
            pair->addView(left);
            if (i + 1 < subjects.size()) {
                auto* right = makeCell(subjects[i + 1]);
                right->setMarginRight(0);
                right->setWidthPercentage(49.0f);
                pair->addView(right);
            }
            listRows_->addView(pair);
        }
    } catch (const std::exception& e) {
        aniswitchStartupLog("HOME: onTrending threw");
        status_->setEmpty(std::string("列表渲染失败: ") + e.what());
        return;
    }
    aniswitchStartupLog("HOME: onTrending done");
    if (coverToken_) brls::cancelDelay(coverToken_);
    coverToken_ = brls::delay(300, [this]() { startCoverLoads(); });
}

void HomeRankFragment::startCoverLoads() {
    aniswitchStartupLog("HOME: cover loads gated by kCoverEnabled");
    (void)covers_;
}

}  // namespace aniswitch
