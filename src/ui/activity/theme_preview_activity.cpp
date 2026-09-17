// SPDX-License-Identifier: AGPL-3.0

#include "ui/activity/theme_preview_activity.hpp"
#include "ui/theme.hpp"
#include "utils/activity_helper.hpp"
#include "utils/config_helper.hpp"
#include <borealis/core/application.hpp>
#include <borealis/core/theme.hpp>
#include <fmt/format.h>

namespace aniswitch {

namespace {

// Tiny helper: read a token from the active theme, fall back
// to a neutral grey if the token is missing (e.g. user is on
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
    // Mirror of the animeko preview: a row of colour swatches
    // for primary / secondary / tertiary + a small sample card
    // showing surface / background / on-* text.  The point is
    // to give the user a visual hint of what the chosen theme
    // looks like before they commit to it.
    auto* swatches = new brls::Box();
    swatches->setAxis(brls::Axis::ROW);
    swatches->setPadding(0, 0, 0, 8);
    swatches->setMarginBottom(8);
    swatches->addView(makeSwatch(tokenColor("ani_accent", nvgRGB(255, 105, 120))));
    swatches->addView(makeSwatch(tokenColor("ani_tag_genre", nvgRGB(60, 80, 120))));
    swatches->addView(makeSwatch(tokenColor("ani_tag_cast",  nvgRGB(80, 60, 100))));
    swatches->addView(makeSwatch(tokenColor("ani_tag_meta",  nvgRGB(50, 110, 80))));
    swatches->addView(makeSwatch(tokenColor("ani_card_highlight",
                                          nvgRGB(255, 200, 80))));
    parent->addView(swatches);

    auto* card = new brls::Box();
    card->setAxis(brls::Axis::COLUMN);
    card->setPadding(16, 12, 16, 12);
    card->setMarginBottom(20);
    card->setBackgroundColor(tokenColor("ani_card_bg", nvgRGB(30, 30, 35)));

    auto* cardTitle = new brls::Label();
    cardTitle->setText("Sample card title");
    cardTitle->setFontSize(16);
    cardTitle->setTextColor(tokenColor("ani_text_primary", nvgRGB(240, 240, 240)));
    card->addView(cardTitle);

    auto* cardSub = new brls::Label();
    cardSub->setText("subtitle text uses ani_text_secondary");
    cardSub->setFontSize(theme::kTypeCaption);
    cardSub->setTextColor(tokenColor("ani_text_secondary", nvgRGB(180, 180, 180)));
    card->addView(cardSub);

    parent->addView(card);
}

}  // namespace

ThemePreviewActivity::ThemePreviewActivity() = default;

void ThemePreviewActivity::onContentAvailable() {
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setPadding(20, 20, 20, 20);

    auto* title = new brls::Label();
    title->setText("主题预览");
    title->setFontSize(24);
    title->setMarginBottom(8);
    root->addView(title);

    auto* cur = new brls::Label();
    const char* variantName =
        brls::Application::getThemeVariant() == brls::ThemeVariant::LIGHT
            ? "亮色" : "暗色";
    cur->setText(fmt::format("当前: {} ({} tokens)", variantName,
                              "ani_*"));
    cur->setFontSize(theme::kTypeCaption);
    cur->setTextColor(aniswitch::theme::kDarkTextSecondary);
    cur->setMarginBottom(20);
    root->addView(cur);

    buildPreviewRow(root);

    // animeko's DarkModeSelectPanel is a 3-radio group with
    // live theme previews in the same row.  We collapse the
    // previews into a single "current" preview above and use
    // three buttons below for the choices — applying one pops
    // + pushes a fresh activity so the swatches repaint.
    auto* hint = new brls::Label();
    hint->setText("选一个: 立即生效, 屏幕会重新进入以更新预览色块。");
    hint->setFontSize(theme::kTypeCaption);
    hint->setTextColor(aniswitch::theme::kDarkTextMuted);
    hint->setMarginBottom(12);
    root->addView(hint);

    auto addButton = [root](const std::string& text, int choice) {
        auto* btn = new brls::Button();
        btn->setText(text);
        btn->setMarginBottom(8);
        btn->registerClickAction(
            [choice](brls::View*) {
                ProgramConfig::instance().setSettingItem<int>(
                    SettingItem::APP_THEME, choice);
                aniswitch::theme::applyTheme(
                    aniswitch::theme::themeChoiceFromIndex(choice));
                brls::Application::popActivity();
                aniswitch::Intent::openThemePreview();
                return true;
            });
        root->addView(btn);
    };
    addButton("自动 (跟系统)", 0);
    addButton("亮色",          1);
    addButton("暗色",          2);

    setContentView(root);
    registerAction("返回", brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });
}

}  // namespace aniswitch
