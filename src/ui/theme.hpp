// SPDX-License-Identifier: AGPL-3.0
//
// ani-switch theme palette + custom theme color registration.
// Applied once on startup (see main.cpp) via
// brls::Theme::getDarkTheme().addColor(...) /
// brls::Theme::getLightTheme().addColor(...).
//
// We extend borealis's default dark and light themes with a
// small set of named tokens so individual UI fragments don't
// have to repeat raw nvgRGB literals.
#pragma once

#include <borealis/core/theme.hpp>
#include <borealis/core/view.hpp>
#include "utils/vibration_helper.hpp"

namespace aniswitch::theme {

// ---- v22 P0: layout / TV-safe constants (1280×720) ----
inline constexpr int kDesignWidth     = 1280;
inline constexpr int kDesignHeight    = 720;
inline constexpr int kSafeMarginX     = 48;
inline constexpr int kSafeMarginTop   = 40;
inline constexpr int kSafeMarginBot   = 40;
inline constexpr int kHeaderHeight    = 72;
inline constexpr int kTabHeight       = 56;
inline constexpr int kHudHeight       = 56;
inline constexpr int kContentWidth    = kDesignWidth - kSafeMarginX * 2;  // 1184

// Type scale (DESIGN.md §4). Interactive text ≥16; buttons/body ≥20.
inline constexpr int kTypeDisplay = 36;
inline constexpr int kTypeH1      = 32;
inline constexpr int kTypeH2      = 24;
inline constexpr int kTypeH3      = 20;
inline constexpr int kTypeBody    = 20;
inline constexpr int kTypeCard    = 18;
inline constexpr int kTypeCaption = 16;
inline constexpr int kTypeMicro   = 14;  // absolute floor, non-interactive only

// List / card geometry
inline constexpr int kPosterW     = 152;
inline constexpr int kPosterH     = 228;
inline constexpr int kRowHeight   = 88;
inline constexpr int kButtonHeight = 56;
inline constexpr float kUnfocusAlpha = 0.82f;
inline constexpr float kFocusStroke = 3.0f;
inline constexpr float kFocusPad    = 6.0f;

// v22: cover downloads — unified switch. v44: turned ON because the
// startup.12 crash was VibrationHelper (now no-op), not ImageLoader.
// SubjectCell / SubjectActivity / poster cards / home rails all gate on this.
inline constexpr bool kCoverEnabled = true;

// Apply TV focus treatment (highlight pad + radius + unfocus dim).
// Borealis View has no setScale — delivery focus is stroke + unfocus
// alpha, not scale (DESIGN.md §0.2 / §10.2).
inline void applyFocusStyle(brls::View* v, float radius = 8.0f,
                            float padding = kFocusPad) {
    if (!v) return;
    v->setHighlightPadding(padding);
    v->setHighlightCornerRadius(radius);
    // Unfocused cards sit at kUnfocusAlpha; focused ones snap to 1.0.
    v->setAlpha(kUnfocusAlpha);
    // GenericEvent is Event<View*> — callback receives the focused view.
    // Vibration tick is intentionally NOT wired here (startup.12 crash
    // hotfix); VibrationHelper::trigger is a no-op until HID is proven.
    v->getFocusEvent()->subscribe([v](brls::View*) {
        v->setAlpha(1.0f);
    });
    v->getFocusLostEvent()->subscribe([v](brls::View*) {
        v->setAlpha(kUnfocusAlpha);
    });
}

// v17.0: theme choice.  The integer is the value stored in the
// config JSON under "appTheme" (matches the SETTING_MAP default
// values in config_helper.cpp so user configs read in via
// load() keep working).
enum class ThemeChoice {
    Auto = 0,  // follows the system; on Switch we currently
                // default to Dark because there's no system
                // color-scheme hook from libnx yet.
    Light = 1,
    Dark = 2,
};

// Brand accent.  Animeko Material seed #4F378B + derived ramp
// for cards / focus / chrome on a 720p Switch screen.
inline const NVGcolor kAccent        = nvgRGB(79, 55, 139);      // #4F378B
inline const NVGcolor kAccentSubtle  = nvgRGB(55, 40, 100);
inline const NVGcolor kAccentSoft    = nvgRGBA(147, 139, 220, 255);
inline const NVGcolor kAccentBright  = nvgRGB(167, 139, 250);    // focus ring
inline const NVGcolor kBrandPink     = nvgRGB(255, 105, 120);

// Chrome surfaces (dark TV UI)
inline const NVGcolor kChromeBg      = nvgRGB(18, 16, 24);
inline const NVGcolor kChromePanel   = nvgRGB(28, 26, 36);
inline const NVGcolor kChromeCard    = nvgRGB(40, 36, 54);
inline const NVGcolor kChromeBorder  = nvgRGBA(255, 255, 255, 28);

inline const NVGcolor kTagGenre      = nvgRGBA(60, 80, 120, 200);
inline const NVGcolor kTagCast       = nvgRGBA(80, 60, 100, 200);
inline const NVGcolor kTagMeta       = nvgRGBA(50, 110, 80, 200);

inline const NVGcolor kDarkCardBg        = nvgRGBA(28, 26, 36, 220);
inline const NVGcolor kDarkCardRowBg     = nvgRGBA(40, 36, 54, 220);
inline const NVGcolor kDarkCardHighlight = nvgRGBA(167, 139, 250, 255);

// DESIGN.md §3.1 semantic colors (missing from early theme.cpp pass).
inline const NVGcolor kRating = nvgRGB(255, 200, 80);   // #FFC850
inline const NVGcolor kError  = nvgRGB(255, 107, 120);  // #FF6B78
inline const NVGcolor kSuccess = nvgRGB(61, 220, 151);  // #3DDC97
inline const NVGcolor kWarning = nvgRGB(255, 200, 80);  // #FFC850
inline const NVGcolor kInfo    = nvgRGB(107, 168, 255); // #6BA8FF

inline const NVGcolor kDarkTextPrimary   = nvgRGB(245, 245, 248);
inline const NVGcolor kDarkTextSecondary = nvgRGB(185, 185, 195);
inline const NVGcolor kDarkTextMuted     = nvgRGB(130, 130, 145);

inline const NVGcolor kLightCardBg        = nvgRGBA(255, 255, 255, 255);
inline const NVGcolor kLightCardRowBg     = nvgRGBA(245, 245, 248, 255);
inline const NVGcolor kLightCardHighlight = nvgRGBA(79, 55, 139, 255);
inline const NVGcolor kLightTextPrimary   = nvgRGB(20, 20, 24);
inline const NVGcolor kLightTextSecondary = nvgRGB(80, 80, 90);
inline const NVGcolor kLightTextMuted     = nvgRGB(140, 140, 150);

// Install the dark palette into brls::Theme::getDarkTheme().
// Idempotent.  Called once at startup.
void registerDarkTheme();

// Install the light palette into brls::Theme::getLightTheme().
// Idempotent.  Called once at startup.
void registerLightTheme();

// v17.0: switch the active theme at runtime.  Picks the dark
// or light token set and writes the same `ani_*` keys to
// brls::Application::setTheme(...) so every component that
// looks up `getTheme()["ani_accent"]` (dark or light) gets
// the right value.  Calling this with the already-active
// theme is a cheap no-op.
void applyTheme(ThemeChoice choice);

// Helper for code that has a raw int from the JSON config
// (which stores the SETTING_MAP index, not the enum value).
inline ThemeChoice themeChoiceFromIndex(int idx) {
    if (idx == 1) return ThemeChoice::Light;
    if (idx == 2) return ThemeChoice::Dark;
    return ThemeChoice::Auto;  // 0 = Auto (default).
}

}  // namespace aniswitch::theme
