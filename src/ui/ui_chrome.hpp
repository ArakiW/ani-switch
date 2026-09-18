// SPDX-License-Identifier: AGPL-3.0
//
// v22 unified page chrome helpers (DESIGN.md §5–§6).
// Every secondary screen should use these instead of ad-hoc
// padding / raw font sizes / missing HUD.
#pragma once

#include <borealis.hpp>
#include <functional>
#include <string>
#include "ui/hud.hpp"
#include "ui/theme.hpp"

namespace aniswitch::chrome {

// Root COLUMN box: TV-safe horizontal margin, design width on Switch,
// v22 dark background.
inline brls::Box* makePageRoot() {
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setPadding(theme::kSafeMarginTop, theme::kSafeMarginX,
                     theme::kSafeMarginBot, theme::kSafeMarginX);
#ifdef __SWITCH__
    root->setWidth(theme::kDesignWidth);
#endif
    root->setBackgroundColor(theme::kChromeBg);
    return root;
}

// Optional compact header bar (secondary pages). Height ~56 with title.
inline brls::Box* makeHeaderBar(const std::string& title,
                                const std::string& right = "") {
    auto* bar = new brls::Box();
    bar->setAxis(brls::Axis::ROW);
    bar->setAlignItems(brls::AlignItems::CENTER);
    bar->setHeight(56);
    bar->setMarginBottom(12);
    bar->setBackgroundColor(theme::kChromePanel);
    bar->setPadding(0, 12, 0, 12);

    auto* t = new brls::Label();
    t->setText(title);
    t->setFontSize(theme::kTypeH2);
    t->setTextColor(theme::kDarkTextPrimary);
    t->setGrow(1.0f);
    bar->addView(t);

    if (!right.empty()) {
        auto* r = new brls::Label();
        r->setText(right);
        r->setFontSize(theme::kTypeCaption);
        r->setTextColor(theme::kDarkTextMuted);
        bar->addView(r);
    }
    return bar;
}

// Screen title (type-h1) or section title (type-h2/h3).
inline brls::Label* makeTitle(const std::string& text,
                              int size = theme::kTypeH2,
                              int marginBottom = 8) {
    auto* t = new brls::Label();
    t->setText(text);
    t->setFontSize(size);
    t->setTextColor(theme::kDarkTextPrimary);
    t->setMarginBottom(marginBottom);
    return t;
}

inline brls::Label* makeSection(const std::string& text,
                                int marginBottom = 8) {
    auto* t = new brls::Label();
    t->setText(text);
    t->setFontSize(theme::kTypeH3);
    t->setTextColor(theme::kAccentBright);
    t->setMarginTop(8);
    t->setMarginBottom(marginBottom);
    return t;
}

inline brls::Label* makeBody(const std::string& text,
                             int marginBottom = 12) {
    auto* t = new brls::Label();
    t->setText(text);
    t->setFontSize(theme::kTypeBody);
    t->setTextColor(theme::kDarkTextPrimary);
    t->setSingleLine(false);
    t->setMarginBottom(marginBottom);
    return t;
}

inline brls::Label* makeCaption(const std::string& text,
                                int marginBottom = 8) {
    auto* t = new brls::Label();
    t->setText(text);
    t->setFontSize(theme::kTypeCaption);
    t->setTextColor(theme::kDarkTextSecondary);
    t->setSingleLine(false);
    t->setMarginBottom(marginBottom);
    return t;
}

inline brls::Label* makeMuted(const std::string& text,
                              int marginBottom = 8) {
    auto* t = new brls::Label();
    t->setText(text);
    t->setFontSize(theme::kTypeCaption);
    t->setTextColor(theme::kDarkTextMuted);
    t->setSingleLine(false);
    t->setMarginBottom(marginBottom);
    return t;
}

// DESIGN.md §6.2 — buttons share 56h + focus treatment.
inline brls::Button* makePrimaryButton(const std::string& text,
                                       std::function<void()> onClick,
                                       int marginBottom = 8) {
    auto* b = new brls::Button();
    b->setText(text);
    b->setHeight(theme::kButtonHeight);
    b->setFontSize(theme::kTypeH3);
    b->setMarginBottom(marginBottom);
    theme::applyFocusStyle(b, 8.0f);
    if (onClick) {
        b->registerClickAction([onClick](brls::View*) {
            onClick();
            return true;
        });
    }
    return b;
}

inline brls::Button* makeSecondaryButton(const std::string& text,
                                         std::function<void()> onClick,
                                         int marginBottom = 8) {
    auto* b = new brls::Button();
    b->setText(text);
    b->setHeight(theme::kButtonHeight);
    b->setFontSize(theme::kTypeH3);
    b->setMarginBottom(marginBottom);
    theme::applyFocusStyle(b, 8.0f);
    if (onClick) {
        b->registerClickAction([onClick](brls::View*) {
            onClick();
            return true;
        });
    }
    return b;
}

// DESIGN.md §5.5 — 88px list row, elevated surface, focus ring.
inline brls::Box* makeListRow(int height = theme::kRowHeight) {
    auto* row = new brls::Box();
    row->setAxis(brls::Axis::ROW);
    row->setHeight(height);
    row->setPadding(16, 16, 16, 16);
    row->setMarginBottom(8);
    row->setAlignItems(brls::AlignItems::CENTER);
    row->setBackground(brls::ViewBackground::SHAPE_COLOR);
    row->setBackgroundColor(theme::kDarkCardRowBg);
    row->setCornerRadius(8);
    row->setFocusable(true);
    theme::applyFocusStyle(row, 8.0f);
#ifdef __SWITCH__
    row->setWidth(theme::kContentWidth);
#endif
    return row;
}

// Standard B-returns Action; pair with registerBack + appendHud.
inline void registerBack(brls::Activity* activity) {
    if (!activity) return;
    activity->registerAction("返回", brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });
}

// Scroll content + HUD shell. Call register* AFTER wrap, then appendHud.
inline brls::Box* attachScrollShell(brls::Activity* activity,
                                    brls::View* scrollableContent) {
    auto* scroll = new brls::ScrollingFrame();
    scroll->setContentView(scrollableContent);
    scroll->setGrow(1.0f);
#ifdef __SWITCH__
    scroll->setWidth(theme::kDesignWidth);
#endif
    return setContentViewWithHudShell(activity, scroll);
}

inline void finish(brls::Activity* activity, brls::Box* shell) {
    appendHud(activity, shell);
}

// Chip (filter / history / tag). selected → accent fill.
inline brls::Box* makeChip(const std::string& text, bool selected,
                           std::function<void()> onClick = nullptr) {
    auto* chip = new brls::Box();
    chip->setFocusable(true);
    chip->setAxis(brls::Axis::ROW);
    chip->setHeight(48);
    chip->setPadding(0, 18, 0, 18);
    chip->setMarginRight(10);
    chip->setMarginBottom(8);
    chip->setAlignItems(brls::AlignItems::CENTER);
    chip->setBackground(brls::ViewBackground::SHAPE_COLOR);
    chip->setCornerRadius(24);
    chip->setBackgroundColor(selected ? theme::kAccent : theme::kChromeCard);
    theme::applyFocusStyle(chip, 24.f);
    auto* lbl = new brls::Label();
    lbl->setText(text);
    lbl->setFontSize(theme::kTypeCaption);
    lbl->setTextColor(selected ? theme::kDarkTextPrimary
                               : theme::kDarkTextSecondary);
    chip->addView(lbl);
    if (onClick) {
        chip->registerClickAction([onClick](brls::View*) {
            onClick();
            return true;
        });
    }
    return chip;
}

// Empty-state / demo banner (collection/history/search pattern).
inline brls::Label* makeDemoBanner(const std::string& text) {
    return makeMuted(text, 8);
}

// Section header + optional caption under it.
inline void addSectionHeader(brls::Box* parent, const std::string& title,
                             const std::string& caption = "") {
    if (!parent) return;
    parent->addView(makeSection(title, caption.empty() ? 8 : 4));
    if (!caption.empty()) parent->addView(makeMuted(caption, 8));
}

// Primary + secondary text column (list rows, person, local video).
inline brls::Box* makeTextColumn(const std::string& title,
                                 const std::string& subtitle = "") {
    auto* col = new brls::Box();
    col->setAxis(brls::Axis::COLUMN);
    col->setGrow(1.0f);
    auto* t = new brls::Label();
    t->setText(title);
    t->setFontSize(theme::kTypeH3);
    t->setTextColor(theme::kDarkTextPrimary);
    t->setSingleLine(true);
    col->addView(t);
    if (!subtitle.empty()) {
        auto* s = new brls::Label();
        s->setText(subtitle);
        s->setFontSize(theme::kTypeCaption);
        s->setTextColor(theme::kDarkTextMuted);
        s->setSingleLine(true);
        col->addView(s);
    }
    return col;
}

}  // namespace aniswitch::chrome
