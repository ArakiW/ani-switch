// SPDX-License-Identifier: AGPL-3.0

#include "ui/activity/theme_preview_activity.hpp"
#include "ui/theme.hpp"
#include "ui/ui_chrome.hpp"
#include "ui/hud.hpp"
#include "utils/activity_helper.hpp"
#include "utils/config_helper.hpp"
#include <borealis/core/application.hpp>
#include <borealis/core/theme.hpp>
#include <fmt/format.h>

namespace aniswitch {

namespace {

// Tiny helper: read a token from the active theme, fall back
// to a theme.hpp token if the key is missing (e.g. user is on
// a theme variant that didn't register our ani_* extension).
NVGcolor tokenColor(const char* token, NVGcolor fallback) {
    auto theme = brls::Application::getTheme();
    auto c = theme.getColor(token);
    if (c.a > 0.0f) return c;
    return fallback;
}

brls::Rectangle* makeSwatch(NVGcolor color, int size = 28) {
    auto* r = new brls::Rectangle();
    r->setSize(brls::Size(size, size));
    r->setColor(color);
    return r;
}

void buildPreviewRow(brls::Box* parent) {
    // Live token swatches — DESIGN.md §3 palette. Fallbacks come from
    // theme.hpp so a missing ani_* key still shows a sane brand color.
    parent->addView(chrome::makeSection("颜色 token", 8));

    auto* swatches = new brls::Box();
    swatches->setAxis(brls::Axis::ROW);
    swatches->setPadding(0, 0, 0, 8);
    swatches->setMarginBottom(8);
    swatches->setAlignItems(brls::AlignItems::CENTER);
    swatches->addView(makeSwatch(tokenColor("ani_accent", theme::kAccent)));
    swatches->addView(makeSwatch(tokenColor("ani_accent_bright", theme::kAccentBright)));
    swatches->addView(makeSwatch(tokenColor("ani_tag_genre", theme::kTagGenre)));
    swatches->addView(makeSwatch(tokenColor("ani_tag_cast", theme::kTagCast)));
    swatches->addView(makeSwatch(tokenColor("ani_tag_meta", theme::kTagMeta)));
    swatches->addView(makeSwatch(tokenColor("ani_card_highlight", theme::kDarkCardHighlight)));

    auto* swatchLabels = new brls::Box();
    swatchLabels->setAxis(brls::Axis::ROW);
    swatchLabels->setMarginBottom(16);
    const char* names[] = {"accent", "focus", "genre", "cast", "meta", "highlight"};
    for (const char* n : names) {
        auto* lbl = chrome::makeMuted(n, 0);
        lbl->setMarginRight(18);
        swatchLabels->addView(lbl);
    }
    parent->addView(swatches);
    parent->addView(swatchLabels);

    parent->addView(chrome::makeSection("样例卡片", 8));

    // Sample card on elevated surface — shows on-* ink tokens.
    auto* card = new brls::Box();
    card->setAxis(brls::Axis::COLUMN);
    card->setPadding(16, 16, 16, 16);
    card->setMarginBottom(20);
    card->setCornerRadius(8);
    card->setBackground(brls::ViewBackground::SHAPE_COLOR);
    card->setBackgroundColor(tokenColor("ani_card_bg", theme::kChromeCard));
#ifdef __SWITCH__
    card->setWidth(theme::kContentWidth);
#endif

    auto* cardTitle = chrome::makeTitle("Sample card title", theme::kTypeH3, 4);
    cardTitle->setTextColor(tokenColor("ani_text_primary", theme::kDarkTextPrimary));
    card->addView(cardTitle);

    auto* cardSub = chrome::makeCaption("subtitle text uses ani_text_secondary", 0);
    cardSub->setTextColor(tokenColor("ani_text_secondary", theme::kDarkTextSecondary));
    card->addView(cardSub);

    parent->addView(card);
}

}  // namespace

ThemePreviewActivity::ThemePreviewActivity() = default;

void ThemePreviewActivity::onContentAvailable() {
    auto* root = chrome::makePageRoot();

    root->addView(chrome::makeTitle("主题预览", theme::kTypeH1, 8));

    const char* variantName =
        brls::Application::getThemeVariant() == brls::ThemeVariant::LIGHT
            ? "亮色" : "暗色";
    root->addView(chrome::makeCaption(
        fmt::format("当前: {} ({} tokens)", variantName, "ani_*"),
        20));

    buildPreviewRow(root);

    // Theme choice chips — animeko DarkModeSelectPanel collapsed to
    // three choices. Applying one pops + pushes so swatches repaint.
    root->addView(chrome::makeSection("主题模式", 8));
    root->addView(chrome::makeMuted(
        "选一个: 立即生效, 屏幕会重新进入以更新预览色块。",
        12));

    const int curChoice =
        ProgramConfig::instance().getSettingItem<int>(SettingItem::APP_THEME, 0);

    auto* chipRow = new brls::Box();
    chipRow->setAxis(brls::Axis::ROW);
    chipRow->setMarginBottom(16);

    auto addChip = [chipRow, curChoice](const std::string& text, int choice) {
        chipRow->addView(chrome::makeChip(
            text, choice == curChoice,
            [choice]() {
                ProgramConfig::instance().setSettingItem<int>(
                    SettingItem::APP_THEME, choice);
                aniswitch::theme::applyTheme(
                    aniswitch::theme::themeChoiceFromIndex(choice));
                brls::Application::popActivity();
                aniswitch::Intent::openThemePreview();
            }));
    };
    addChip("自动 (跟系统)", 0);
    addChip("亮色", 1);
    addChip("暗色", 2);
    root->addView(chipRow);

    auto* shell = chrome::attachScrollShell(this, root);
    chrome::registerBack(this);
    chrome::finish(this, shell);
}

}  // namespace aniswitch
