// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
#include "utils/vibration_helper.hpp"

namespace aniswitch {

namespace {
// v22 crash hotfix (startup.12): sendRumble + detached thread on first
// focus was the top suspect for "trends 200 → userAppExit" with no
// onTrending breadcrumb. Keep the API; force a safe no-op until HID
// vibration device init is proven on device.
bool gVibrationEnabled = false;
}  // namespace

void VibrationHelper::setEnabled(bool enabled) { gVibrationEnabled = enabled; }
bool VibrationHelper::isEnabled() { return gVibrationEnabled; }

void VibrationHelper::trigger(int duration) {
    // Intentionally a no-op. Re-enable only after a dedicated on-device
    // rumble smoke test (DESIGN.md §7.5 is a P2 nice-to-have, not a
    // stability blocker).
    (void)duration;
    (void)gVibrationEnabled;
}

}  // namespace aniswitch
