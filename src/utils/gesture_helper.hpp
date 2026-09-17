// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
#pragma once

#include <borealis/core/box.hpp>
#include <functional>
#include <string>

namespace aniswitch {

class GestureHelper {
public:
    // Wire a touch-swipe detector onto a brls::Box.
    // Calls on_swipe(direction) on recognized swipes; ignores micro-movements.
    enum class Direction { LEFT, RIGHT, UP, DOWN };
    using SwipeCallback = std::function<void(Direction)>;

    static void attach(brls::Box* target, SwipeCallback cb);
};

}  // namespace aniswitch
