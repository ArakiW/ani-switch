// SPDX-License-Identifier: AGPL-3.0
//
// v18.7: animeko 6.1.0 `ui-settings/tabs/theme/ThemePreviewPanel`
// + `DarkModeSelectPanel` ported as a single activity.  Renders
// the current theme's tokens (primary/secondary/tertiary/surface
// /background + on-* text colors) as a row of colour swatches,
// then offers three buttons (auto/light/dark) that re-skin the
// whole UI immediately and repaint the swatches in place.
//
// borealis 5f08b286 doesn't expose a theme-changed observer, so
// the repaint trick is: after applyTheme() pops the activity
// and pushes a fresh ThemePreviewActivity.  That double-stack
// dance is what animeko does too (it lives inside a
// `CompositionLocal` that auto-recomposes).

#pragma once

#include <borealis.hpp>

namespace aniswitch {

class ThemePreviewActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/theme_preview_activity.xml");
    ThemePreviewActivity();
    void onContentAvailable() override;
};

}  // namespace aniswitch
