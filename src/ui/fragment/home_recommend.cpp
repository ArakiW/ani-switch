// SPDX-License-Identifier: AGPL-3.0
#include "ui/fragment/home_recommend.hpp"
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

namespace aniswitch {

HomeRecommendFragment::HomeRecommendFragment() {
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setPadding(0, 0, theme::kHudHeight / 2, 0);
    root->setBackgroundColor(theme::kChromeBg);

    auto* title = new brls::Label();
    title->setText("推荐");
    title->setFontSize(theme::kTypeH2);
    title->setTextColor(theme::kDarkTextPrimary);
    title->setSingleLine(true);
    title->setMarginBottom(4);
    root->addView(title);
    auto* hint = new brls::Label();
    hint->setText("你可能喜欢 · Ani /v1/trends");
    hint->setFontSize(theme::kTypeCaption);
    hint->setTextColor(theme::kDarkTextMuted);
    hint->setSingleLine(true);
    hint->setMarginBottom(10);
    root->addView(hint);

    status_ = new StatusView();
    status_->setLoading("加载推荐...");
    status_->setGrow(1);
    root->addView(status_);

    railList_ = new brls::Box();
    railList_->setAxis(brls::Axis::ROW);
    railList_->setPadding(8, 8, 12, 0);
    railList_->setVisibility(brls::Visibility::GONE);
    auto* rail = new brls::HScrollingFrame();
    rail->setContentView(railList_);
    rail->setHeight(theme::kPosterH + 28);
    rail->setWidthPercentage(100.0f);
    rail->setGrow(0);
    rail->setScrollingIndicatorVisible(false);
    root->addView(rail);

    auto* moreLabel = new brls::Label();
    moreLabel->setText("全部条目");
    moreLabel->setFontSize(theme::kTypeH3);
    moreLabel->setSingleLine(true);
    moreLabel->setMarginTop(4);
    moreLabel->setMarginBottom(6);
    root->addView(moreLabel);

    list_ = new brls::Box();
    list_->setAxis(brls::Axis::COLUMN);
    list_->setVisibility(brls::Visibility::GONE);
    auto* scroll = new brls::ScrollingFrame();
    scroll->setContentView(list_);
    scroll->setGrow(1);
    root->addView(scroll);
    addView(root);

    presenter_.onTrending.subscribe([this](std::vector<SearchSubject> v) {
        loading_ = false;
        list_->clearViews();
        railList_->clearViews();
        if (v.empty()) {
            status_->setEmpty("暂无推荐\n" + HTTP::proxyHint());
            return;
        }
        if (v.size() > 20) v.resize(20);
        status_->setVisibility(brls::Visibility::GONE);
        list_->setVisibility(brls::Visibility::VISIBLE);
        railList_->setVisibility(brls::Visibility::VISIBLE);
        try {
            for (const auto& s : v) {
                const int32_t id = s.id;
                railList_->addView(makePosterCardFromSearch(s, [id]() {
                    Intent::openSubject(id);
                }));
            }
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
                        fmt::format("cell-{}", id));
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
            for (size_t i = 0; i < v.size(); i += 2) {
                auto* pair = new brls::Box();
                pair->setAxis(brls::Axis::ROW);
#ifdef __SWITCH__
                pair->setWidth(theme::kContentWidth);
#endif
                auto* left = makeCell(v[i]);
                left->setWidthPercentage(49.0f);
                pair->addView(left);
                if (i + 1 < v.size()) {
                    auto* right = makeCell(v[i + 1]);
                    right->setMarginRight(0);
                    right->setWidthPercentage(49.0f);
                    pair->addView(right);
                }
                list_->addView(pair);
            }
        } catch (const std::exception& e) {
            status_->setEmpty(std::string("列表渲染失败: ") + e.what());
        }
    });
    refresh();
    if (timeoutToken_) brls::cancelDelay(timeoutToken_);
    timeoutToken_ = brls::delay(8000, [this]() {
        if (!loading_) return;
        status_->setEmpty("加载超时\n" + HTTP::proxyHint());
    });
}

HomeRecommendFragment::~HomeRecommendFragment() {
    if (timeoutToken_) {
        brls::cancelDelay(timeoutToken_);
        timeoutToken_ = 0;
    }
}

void HomeRecommendFragment::refresh() {
    loading_ = true;
    status_->setLoading("加载推荐...");
    list_->setVisibility(brls::Visibility::GONE);
    railList_->setVisibility(brls::Visibility::GONE);
    presenter_.fetchTrending("推荐");
}

}  // namespace aniswitch
