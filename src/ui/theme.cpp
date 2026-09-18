// SPDX-License-Identifier: AGPL-3.0
#include "ui/theme.hpp"
#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>

namespace aniswitch::theme {

void registerDarkTheme() {
    // v22 P0: 3px focus stroke (borealis default is 5).
    brls::Application::getStyle().addMetric("brls/highlight/stroke_width",
                                            kFocusStroke);

    auto& dark = brls::Theme::getDarkTheme();
    dark.addColor("ani_accent",         kAccent);
    dark.addColor("ani_accent_subtle",  kAccentSubtle);
    dark.addColor("ani_accent_soft",    kAccentSoft);
    dark.addColor("ani_accent_bright",  kAccentBright);
    dark.addColor("ani_focus_ring",     kAccentBright);
    dark.addColor("ani_bg",             kChromeBg);
    dark.addColor("ani_surface",        kChromePanel);
    dark.addColor("ani_surface_elevated", kChromeCard);
    dark.addColor("ani_card_bg",        kDarkCardBg);
    dark.addColor("ani_card_row",       kDarkCardRowBg);
    dark.addColor("ani_card_highlight", kDarkCardHighlight);
    dark.addColor("ani_text_primary",   kDarkTextPrimary);
    dark.addColor("ani_text_secondary", kDarkTextSecondary);
    dark.addColor("ani_text_muted",     kDarkTextMuted);
    dark.addColor("ani_tag_genre",      kTagGenre);
    dark.addColor("ani_tag_cast",       kTagCast);
    dark.addColor("ani_tag_meta",       kTagMeta);
    // DESIGN.md §3.1 semantic status colors.
    dark.addColor("ani_rating",         kRating);
    dark.addColor("ani_error",          kError);
    dark.addColor("ani_success",        kSuccess);
    dark.addColor("ani_warning",        kWarning);
    dark.addColor("ani_info",           kInfo);
    dark.addColor("ani_ink",            kDarkTextPrimary);
    dark.addColor("ani_ink_secondary",  kDarkTextSecondary);
    dark.addColor("ani_ink_muted",      kDarkTextMuted);
    // TsVitch player port: OSD bottom bar accent.
    dark.addColor("color/tsvitch", kAccentBright);
    // v22 P0: TV focus — brand purple ring instead of borealis cyan.
    dark.addColor("brls/highlight/color1", kAccentBright);
    dark.addColor("brls/highlight/color2", nvgRGB(196, 181, 253));
    dark.addColor("brls/highlight/background", nvgRGBA(167, 139, 250, 48));
    dark.addColor("brls/accent", kAccentBright);
    dark.addColor("brls/button/primary_enabled_background", kAccent);
    dark.addColor("brls/button/primary_enabled_text", kDarkTextPrimary);
}

void registerLightTheme() {
    auto& light = brls::Theme::getLightTheme();
    light.addColor("ani_accent",         kAccent);
    light.addColor("ani_accent_subtle",  kAccentSubtle);
    light.addColor("ani_accent_soft",    kAccentSoft);
    light.addColor("ani_accent_bright",  kAccentBright);
    light.addColor("ani_focus_ring",     nvgRGB(91, 63, 168));
    light.addColor("ani_bg",             nvgRGB(247, 247, 250));
    light.addColor("ani_surface",        nvgRGB(255, 255, 255));
    light.addColor("ani_surface_elevated", nvgRGB(240, 240, 245));
    light.addColor("ani_card_bg",        kLightCardBg);
    light.addColor("ani_card_row",       kLightCardRowBg);
    light.addColor("ani_card_highlight", kLightCardHighlight);
    light.addColor("ani_text_primary",   kLightTextPrimary);
    light.addColor("ani_text_secondary", kLightTextSecondary);
    light.addColor("ani_text_muted",     kLightTextMuted);
    light.addColor("ani_tag_genre",      kTagGenre);
    light.addColor("ani_tag_cast",       kTagCast);
    light.addColor("ani_tag_meta",       kTagMeta);
    light.addColor("color/tsvitch", kAccent);
    light.addColor("brls/highlight/color1", nvgRGB(91, 63, 168));
    light.addColor("brls/highlight/color2", kAccent);
    light.addColor("brls/highlight/background", nvgRGBA(79, 55, 139, 32));
    light.addColor("brls/accent", kAccent);
}

void applyTheme(ThemeChoice choice) {
    // Resolve Auto to a concrete choice.  Switch newlib has no
    // system color-scheme hook from libnx today (v17.0), so
    // Auto falls through to Dark — that matches the historical
    // ani-switch default and keeps the color contract consistent
    // with what the user had pre-v17.0.  A later v17.x will
    // wire up libnx color-set support.
    ThemeChoice actual = choice;
    if (actual == ThemeChoice::Auto) actual = ThemeChoice::Dark;

    // The vendored borealis 5f08b286 has only the *read* side of
    // the theme API (`brls::Application::getTheme` /
    // `getThemeVariant`); there is no `setTheme` to flip the
    // active palette at runtime.  Until we bump to a newer
    // borealis (the upstream wiliwili + ani builds against a
    // commit that adds `setThemeVariant`) we work around it by
    // writing the chosen palette into *both* the dark and light
    // ThemeValues stores under the same `ani_*` keys.  That
    // way fragments that look up `getTheme()["ani_card_bg"]` get
    // the right color for the active variant, and the saved
    // choice persists across launches.  The remaining limitation
    // is that non-`ani_*` borealis colors (background, foreground,
    // etc.) still flip with the active ThemeVariant — we can't
    // override those without a setTheme hook.
    auto& dark  = brls::Theme::getDarkTheme();
    auto& light = brls::Theme::getLightTheme();
    if (actual == ThemeChoice::Light) {
        light.addColor("ani_accent",         kAccent);
        light.addColor("ani_accent_subtle",  kAccentSubtle);
        light.addColor("ani_card_bg",        kLightCardBg);
        light.addColor("ani_card_row",       kLightCardRowBg);
        light.addColor("ani_card_highlight", kLightCardHighlight);
        light.addColor("ani_text_primary",   kLightTextPrimary);
        light.addColor("ani_text_secondary", kLightTextSecondary);
        light.addColor("ani_text_muted",     kLightTextMuted);
        light.addColor("ani_tag_genre",      kTagGenre);
        light.addColor("ani_tag_cast",       kTagCast);
        light.addColor("ani_tag_meta",       kTagMeta);
        // Also re-apply the dark token set under its own keys so
        // the future setTheme-active path is consistent.  Today's
        // display only reads the active variant's keys, but if the
        // user later switches we want both stores in sync.
        dark.addColor("ani_accent",         kAccent);
        dark.addColor("ani_card_bg",        kDarkCardBg);
        dark.addColor("ani_card_row",       kDarkCardRowBg);
        dark.addColor("ani_card_highlight", kDarkCardHighlight);
        dark.addColor("ani_text_primary",   kDarkTextPrimary);
        dark.addColor("ani_text_secondary", kDarkTextSecondary);
        dark.addColor("ani_text_muted",     kDarkTextMuted);
    } else {
        dark.addColor("ani_accent",         kAccent);
        dark.addColor("ani_accent_subtle",  kAccentSubtle);
        dark.addColor("ani_card_bg",        kDarkCardBg);
        dark.addColor("ani_card_row",       kDarkCardRowBg);
        dark.addColor("ani_card_highlight", kDarkCardHighlight);
        dark.addColor("ani_text_primary",   kDarkTextPrimary);
        dark.addColor("ani_text_secondary", kDarkTextSecondary);
        dark.addColor("ani_text_muted",     kDarkTextMuted);
        dark.addColor("ani_tag_genre",      kTagGenre);
        dark.addColor("ani_tag_cast",       kTagCast);
        dark.addColor("ani_tag_meta",       kTagMeta);
        light.addColor("ani_accent",         kAccent);
        light.addColor("ani_card_bg",        kLightCardBg);
        light.addColor("ani_card_row",       kLightCardRowBg);
        light.addColor("ani_card_highlight", kLightCardHighlight);
        light.addColor("ani_text_primary",   kLightTextPrimary);
        light.addColor("ani_text_secondary", kLightTextSecondary);
        light.addColor("ani_text_muted",     kLightTextMuted);
    }
    brls::Logger::info("aniswitch theme -> {}",
        actual == ThemeChoice::Light ? "light" : "dark");
}

}  // namespace aniswitch::theme
