// SPDX-License-Identifier: AGPL-3.0
//
// v22 P1: bottom controller HUD driven by the current activity's
// registered ActionMap. Activities register actions via
// Activity::registerAction; the HUD renders chips from
// contentView->getActions() so copy is never hardcoded per screen.
#pragma once

#include <borealis.hpp>
#include <string>
#include <vector>
#include "ui/theme.hpp"

namespace aniswitch {

struct ActionHint {
    std::string button;  // "A" | "B" | "X" | "Y" | "L" | "R" | "+"
    std::string label;
};

using ActionMap = std::vector<ActionHint>;

// Map borealis ControllerButton to a short HUD glyph.
inline std::string buttonDisplayName(int button) {
    switch (button) {
        case brls::BUTTON_A:     return "A";
        case brls::BUTTON_B:     return "B";
        case brls::BUTTON_X:     return "X";
        case brls::BUTTON_Y:     return "Y";
        case brls::BUTTON_LB:    return "L";
        case brls::BUTTON_RB:    return "R";
        case brls::BUTTON_LT:    return "ZL";
        case brls::BUTTON_RT:    return "ZR";
        case brls::BUTTON_START: return "+";
        case brls::BUTTON_BACK:  return "-";
        default:                 return "?";
    }
}

// Build ActionMap from a view's registered actions (typically the
// activity content view after all registerAction calls have run).
inline ActionMap actionMapFromView(brls::View* view) {
    ActionMap map;
    if (!view) return map;
    for (const auto& a : view->getActions()) {
        if (!a || a->isHidden() || !a->isAvailable()) continue;
        if (a->getType() != brls::ACTION_GAMEPAD) continue;
        const std::string hint = a->getHintText();
        if (hint.empty()) continue;
        map.push_back({buttonDisplayName(a->getButton()), hint});
    }
    return map;
}

// Bottom HUD bar, fixed height. Parent should place it last in a COLUMN.
inline brls::Box* buildHud(const ActionMap& map) {
    auto* hud = new brls::Box();
    hud->setAxis(brls::Axis::ROW);
    hud->setHeight(theme::kHudHeight);
    hud->setPadding(0, theme::kSafeMarginX, 0, theme::kSafeMarginX);
    hud->setAlignItems(brls::AlignItems::CENTER);
    hud->setBackgroundColor(theme::kChromePanel);

    for (const auto& h : map) {
        auto* chip = new brls::Box();
        chip->setAxis(brls::Axis::ROW);
        chip->setAlignItems(brls::AlignItems::CENTER);
        chip->setMarginRight(20);
        chip->setHeight(28);

        auto* keyBox = new brls::Box();
        keyBox->setAxis(brls::Axis::ROW);
        keyBox->setAlignItems(brls::AlignItems::CENTER);
        keyBox->setJustifyContent(brls::JustifyContent::CENTER);
        keyBox->setBackground(brls::ViewBackground::SHAPE_COLOR);
        keyBox->setBackgroundColor(theme::kAccent);
        keyBox->setCornerRadius(14);
        keyBox->setWidth(36);
        keyBox->setHeight(28);
        auto* key = new brls::Label();
        key->setText(h.button);
        key->setFontSize(theme::kTypeCaption);
        key->setTextColor(theme::kDarkTextPrimary);
        keyBox->addView(key);
        chip->addView(keyBox);

        auto* lab = new brls::Label();
        lab->setText(" " + h.label);
        lab->setFontSize(theme::kTypeCaption);
        lab->setTextColor(theme::kDarkTextSecondary);
        chip->addView(lab);

        hud->addView(chip);
    }

    auto* spacer = new brls::Box();
    spacer->setGrow(1.0f);
    hud->addView(spacer);
    return hud;
}

// Preferred entry: HUD chips generated from the activity's ActionMap.
inline brls::Box* buildHudFromActions(brls::View* activityContentView) {
    return buildHud(actionMapFromView(activityContentView));
}

// Wrap a scrollable (or any) content view in a COLUMN shell and
// setContentView it. Call registerAction(...) next, then appendHud().
// DESIGN.md §6.4: HUD must sit outside the scroll frame.
// startup.13: wrap/scroll MUST get an explicit Switch width — without
// it the ScrollingFrame collapses and the detail page renders empty.
inline brls::Box* setContentViewWithHudShell(brls::Activity* activity,
                                             brls::View* scrollable) {
    auto* wrap = new brls::Box();
    wrap->setAxis(brls::Axis::COLUMN);
#ifdef __SWITCH__
    wrap->setWidth(theme::kDesignWidth);
#endif
    if (scrollable) {
        scrollable->setGrow(1.0f);
#ifdef __SWITCH__
        scrollable->setWidth(theme::kDesignWidth);
#endif
        wrap->addView(scrollable);
    }
    activity->setContentView(wrap);
    return wrap;
}

// Append the HUD bar AFTER all registerAction calls so chips match
// the live ActionMap.
inline void appendHud(brls::Activity* activity, brls::Box* wrap) {
    if (!activity || !wrap) return;
    wrap->addView(buildHudFromActions(activity->getContentView()));
}

}  // namespace aniswitch
