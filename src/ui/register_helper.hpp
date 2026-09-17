// SPDX-License-Identifier: AGPL-3.0
//
// ani-switch PATCH (2026-09-02): wiliwili's register_helper registers
// every fragment/presenter for the B站 app. Our app is a Bangumi client
// with a different fragment set, so we provide a minimal stub here that
// the real main.cpp can call. The full wiliwili implementation is at
// wiliwili-reference/wiliwili/source/utils/register_helper.cpp.

#pragma once

#include <borealis.hpp>

namespace aniswitch {

// Register any application-specific views / activities with brls.
// In the full wiliwili this would register every fragment XML; here
// we register nothing extra because our main activity is set up
// programmatically in main_activity.cpp.
inline void registerViewsForXML() {}

// wiliwili calls this from main() to populate brls::Application's
// view registry before the first activity runs. Keeping the same
// symbol so main.cpp doesn't need a different signature.
inline void registerAllViews() { registerViewsForXML(); }

}  // namespace aniswitch
