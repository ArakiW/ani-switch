// SPDX-License-Identifier: AGPL-3.0

#include "ui/activity/search_activity.hpp"
#include "ui/theme.hpp"
#include "ui/poster_card.hpp"
#include "ui/hud.hpp"
#include "utils/activity_helper.hpp"
#include "utils/string_helper.hpp"
#include "utils/number_helper.hpp"
#include <borealis/core/logger.hpp>
#include <borealis/views/edit_text_dialog.hpp>
#include <fmt/format.h>
#include <borealis/views/scrolling_frame.hpp>

namespace aniswitch {

SearchActivity::SearchActivity(const std::string& initialQuery)
    : initial_(initialQuery) {}

void SearchActivity::onContentAvailable() {
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setPadding(20, theme::kSafeMarginX, 20, theme::kSafeMarginX);

    auto* input = new brls::Button();
    input->setText("输入关键词...");
    input->setHeight(theme::kButtonHeight);
    input->setMarginBottom(10);
    theme::applyFocusStyle(input);
    input->registerClickAction([this](brls::View*) {
        showInput();
        return true;
    });
    root->addView(input);

    auto* historyHeader = new brls::Label();
    historyHeader->setText("搜索历史");
    historyHeader->setFontSize(theme::kTypeH2);
    historyHeader->setMarginBottom(6);
    root->addView(historyHeader);

    auto* historyList = new brls::Box();
    historyList->setAxis(brls::Axis::ROW);  // v22: history as chips
    historyList->setAlignItems(brls::AlignItems::CENTER);
    historyList->setId("history_list");
    root->addView(historyList);

    auto* resultsHeader = new brls::Label();
    resultsHeader->setText("搜索结果");
    resultsHeader->setFontSize(theme::kTypeH2);
    resultsHeader->setMarginTop(16);
    resultsHeader->setMarginBottom(6);
    root->addView(resultsHeader);

    // v22 §5.4: 6-col poster grid (wrapped ROW boxes).
    auto* resultsList = new brls::Box();
    resultsList->setAxis(brls::Axis::COLUMN);
    resultsList->setId("results_list");
    root->addView(resultsList);

    auto* scroll = new brls::ScrollingFrame();
    scroll->setContentView(root);
    // v22 §6.4: HUD shell outside the scroll frame.
    auto* shell = setContentViewWithHudShell(this, scroll);
    registerAction("返回", brls::BUTTON_B, [](brls::View*) { brls::Application::popActivity(); return true; });
    registerAction("搜索", brls::BUTTON_Y, [this](brls::View*) {
        showInput();
        return true;
    });
    appendHud(this, shell);
    root_ = root;

    presenter_.onHistory.subscribe([this](std::vector<std::string> v) { onHistory(v); });
    presenter_.onResults.subscribe([this](std::vector<SearchSubject> v) { onResults(v); });
    presenter_.loadHistory();

    if (!initial_.empty()) {
        doSearch(initial_);
    }
}

void SearchActivity::onHistory(std::vector<std::string> hist) {
    auto* list = dynamic_cast<brls::Box*>(root_->getView("history_list"));
    if (!list) return;
    list->clearViews();
    if (hist.empty()) {
        auto* empty = new brls::Label();
        empty->setText("(无)");
        empty->setFontSize(theme::kTypeCaption);
        list->addView(empty);
        return;
    }
    for (const auto& q : hist) {
        auto* chip = new brls::Box();
        chip->setFocusable(true);
        chip->setAxis(brls::Axis::ROW);
        chip->setHeight(48);
        chip->setPadding(0, 18, 0, 18);
        chip->setMarginRight(10);
        chip->setMarginBottom(8);
        chip->setBackground(brls::ViewBackground::SHAPE_COLOR);
        chip->setBackgroundColor(theme::kChromeCard);
        chip->setCornerRadius(24);
        theme::applyFocusStyle(chip, 24.f);
        auto* lbl = new brls::Label();
        lbl->setText(q);
        lbl->setFontSize(theme::kTypeCaption);
        chip->addView(lbl);
        std::string qcopy = q;
        chip->registerClickAction([this, qcopy](brls::View*) {
            doSearch(qcopy);
            return true;
        });
        list->addView(chip);
    }
}

void SearchActivity::onResults(std::vector<SearchSubject> res) {
    auto* list = dynamic_cast<brls::Box*>(root_->getView("results_list"));
    if (!list) return;
    list->clearViews();
    if (res.empty()) {
        auto* empty = new brls::Label();
        empty->setText("(无结果)");
        empty->setFontSize(theme::kTypeBody);
        list->addView(empty);
        return;
    }
    // 6 columns × poster 152×228, gap 16 (DESIGN.md §5.4).
    constexpr int kCols = 6;
    for (size_t i = 0; i < res.size(); i += kCols) {
        auto* row = new brls::Box();
        row->setAxis(brls::Axis::ROW);
        row->setMarginBottom(24);
        for (size_t j = 0; j < kCols && i + j < res.size(); ++j) {
            const auto& s = res[i + j];
            int32_t sid = s.id;
            auto* cell = makePosterCardFromSearch(s, [sid]() {
                Intent::openSubject(sid);
            });
            // v22 §7.2.6: remember focus for restore after detail pop.
            cell->getFocusEvent()->subscribe([this, cell](brls::View*) {
                lastFocused_ = cell;
            });
            row->addView(cell);
        }
        list->addView(row);
    }
}

void SearchActivity::onResume() {
    if (lastFocused_) {
        brls::Application::giveFocus(lastFocused_);
    }
}

void SearchActivity::showInput() {
    // brls 5f08b286 doesn't have a static EditTextDialog::open() helper.
    // Use the application's ImeManager.openForText() with a callback
    // that gets the submitted text directly.
    auto* ime = brls::Application::getImeManager();
    if (!ime) return;
    ime->openForText([this](std::string text) {
        if (!text.empty()) doSearch(text);
    }, "搜索番剧", "", 64);
}

void SearchActivity::doSearch(const std::string& q) {
    presenter_.remember(q);
    presenter_.search(q);
}

}  // namespace aniswitch
