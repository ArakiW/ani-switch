// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
#include "utils/gesture_helper.hpp"
#include <borealis/core/logger.hpp>

namespace aniswitch {

void GestureHelper::attach(brls::Box* target, SwipeCallback cb) {
    if (!target || !cb) return;
    // borealis already routes HID touch input. The full wiliwili version
    // implements directional swipe recognition; we keep a stub here and
    // rely on default brls gesture handling for now.
    brls::Logger::debug("GestureHelper::attach: target registered (stub)");
}

}  // namespace aniswitch
