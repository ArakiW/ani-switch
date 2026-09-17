// SPDX-License-Identifier: AGPL-3.0

#include "ui/activity/my_collection_activity.hpp"
#include "ui/theme.hpp"
#include "ui/poster_card.hpp"
#include "ui/hud.hpp"
#include "ui/demo_data.hpp"
#include "utils/activity_helper.hpp"
#include "utils/number_helper.hpp"
#include "utils/config_helper.hpp"
#include <borealis/core/logger.hpp>
#include <fmt/format.h>
#include <borealis/views/scrolling_frame.hpp>

namespace aniswitch {

MyCollectionActivity::MyCollectionActivity() = default;

void MyCollectionActivity::onContentAvailable() {
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setPadding(20, theme::kSafeMarginX, 20, theme::kSafeMarginX);

    auto* headerRow = new brls::Box();
    headerRow->setAxis(brls::Axis::ROW);
    headerRow->setMarginBottom(10);

    auto* title = new brls::Label();
    title->setText("我的追番");
    title->setFontSize(theme::kTypeH2);
    title->setGrow(1.0f);
    headerRow->addView(title);

    auto* btnRefresh = new brls::Button();
    btnRefresh->setText("同步");
    btnRefresh->setHeight(theme::kButtonHeight);
    theme::applyFocusStyle(btnRefresh);
    btnRefresh->registerClickAction([this](brls::View*) {
        if (ProgramConfig::instance().hasLoginInfo()) {
            presenter_.refreshFromRemote();
        } else {
            brls::Logger::info("Not logged in; falling back to local cache");
            presenter_.refreshFromLocal();
        }
        return true;
    });
    headerRow->addView(btnRefresh);
    root->addView(headerRow);

    // v22: 5-state filter chips (全部/想看/在看/看过/搁置/抛弃).
    filterRow_ = new brls::Box();
    filterRow_->setAxis(brls::Axis::ROW);
    filterRow_->setMarginBottom(16);
    root->addView(filterRow_);
    renderFilterChips();

    auto* list = new brls::Box();
    list->setAxis(brls::Axis::COLUMN);
    list->setId("collection_list");
    root->addView(list);
    list_ = list;

    auto* scroll = new brls::ScrollingFrame();
    scroll->setContentView(root);
    auto* shell = setContentViewWithHudShell(this, scroll);
    registerAction("返回", brls::BUTTON_B, [](brls::View*) { brls::Application::popActivity(); return true; });
    registerAction("同步", brls::BUTTON_Y, [this](brls::View*) {
        if (ProgramConfig::instance().hasLoginInfo()) {
            presenter_.refreshFromRemote();
        } else {
            presenter_.refreshFromLocal();
        }
        return true;
    });
    appendHud(this, shell);

    presenter_.onCollection.subscribe([this](std::vector<SQLiteStore::CollectionEntry> v) {
        cache_ = v;
        render(v);
    });
    presenter_.onError.subscribe([this](const std::string& m) {
        brls::Logger::warning("MyCollection: {}", m);
        // v22: remote/local failure still fills the grid.
        cache_.clear();
        render(cache_);
    });
    if (ProgramConfig::instance().hasLoginInfo()) {
        presenter_.refreshFromRemote();
    } else {
        // v22 compose-next: not logged in — do not call refreshFromLocal
        // first (it can sync-render empty and drop the banner). Render
        // demo directly so the banner stays.
        render(demo::collection());
    }
}

void MyCollectionActivity::renderFilterChips() {
    if (!filterRow_) return;
    filterRow_->clearViews();
    struct Chip {
        const char* label;
        int32_t type;  // -1 = all
    };
    static const Chip kChips[] = {
        {"全部", -1}, {"想看", 1},  {"在看", 2},
        {"看过", 3},  {"搁置", 4},  {"抛弃", 5},
    };
    for (const auto& c : kChips) {
        auto* chip = new brls::Box();
        chip->setFocusable(true);
        chip->setAxis(brls::Axis::ROW);
        chip->setHeight(48);
        chip->setPadding(0, 18, 0, 18);
        chip->setMarginRight(10);
        chip->setBackground(brls::ViewBackground::SHAPE_COLOR);
        chip->setCornerRadius(24);
        const bool selected = (filterType_ == c.type);
        chip->setBackgroundColor(selected ? theme::kAccent : theme::kChromeCard);
        theme::applyFocusStyle(chip, 24.f);
        auto* lbl = new brls::Label();
        lbl->setText(c.label);
        lbl->setFontSize(theme::kTypeCaption);
        chip->addView(lbl);
        const int32_t t = c.type;
        chip->registerClickAction([this, t](brls::View*) {
            filterType_ = t;
            renderFilterChips();
            render(cache_);
            return true;
        });
        filterRow_->addView(chip);
    }
}

void MyCollectionActivity::render(const std::vector<SQLiteStore::CollectionEntry>& v) {
    if (!list_) return;
    list_->clearViews();

    std::vector<const SQLiteStore::CollectionEntry*> filtered;
    for (const auto& e : v) {
        if (filterType_ < 0 || e.type == filterType_) filtered.push_back(&e);
    }

    const bool useDemo = filtered.empty();
    if (useDemo) {
        // v22 compose-next: empty → demo grid under banner.
        auto* banner = new brls::Label();
        banner->setText(demo::kBanner);
        banner->setFontSize(theme::kTypeCaption);
        banner->setTextColor(theme::kDarkTextMuted);
        banner->setMarginBottom(8);
        list_->addView(banner);
        static const auto kDemo = demo::collection();
        for (const auto& e : kDemo) {
            if (filterType_ < 0 || e.type == filterType_) filtered.push_back(&e);
        }
        if (filtered.empty()) {
            for (const auto& e : kDemo) filtered.push_back(&e);
        }
    }

    // v22 §5.4: 6-col poster grid.
    constexpr int kCols = 6;
    for (size_t i = 0; i < filtered.size(); i += kCols) {
        auto* row = new brls::Box();
        row->setAxis(brls::Axis::ROW);
        row->setMarginBottom(24);
        for (size_t j = 0; j < kCols && i + j < filtered.size(); ++j) {
            const auto& e = *filtered[i + j];
            int32_t sid = e.subjectId;
            const std::string name = e.nameCN.empty() ? e.name : e.nameCN;
            auto* cell = makePosterCard(
                e.subjectId, name, static_cast<float>(e.rating), e.cover,
                theme::kPosterW, theme::kPosterH,
                [sid]() { Intent::openSubject(sid); });
            cell->getFocusEvent()->subscribe([this, cell](brls::View*) {
                lastFocused_ = cell;
            });
            row->addView(cell);
        }
        list_->addView(row);
    }
}

void MyCollectionActivity::onResume() {
    if (lastFocused_) {
        brls::Application::giveFocus(lastFocused_);
    }
}

}  // namespace aniswitch
