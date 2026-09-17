// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
#pragma once

namespace aniswitch {

class VibrationHelper {
public:
    // duration: 0 = off, 1 = short tick (focus), 2 = medium (success),
    // 3 = long impact (destructive confirm).
    static void trigger(int duration = 1);

    // Global on/off (design §7.5 "可关").
    static void setEnabled(bool enabled);
    static bool isEnabled();
};

}  // namespace aniswitch
