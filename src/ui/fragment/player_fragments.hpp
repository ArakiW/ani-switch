// SPDX-License-Identifier: AGPL-3.0
//
// Player-side fragments: danmaku input, quality selector, speed
// selector. Stub for v0.1 — the real player renders these as a single
// overlay; once we move the overlay to a Fragment-based architecture
// these classes will own the rendering.
#pragma once
#include <borealis.hpp>

namespace aniswitch {
class DanmakuInputFragment : public brls::Box {
public:
    DanmakuInputFragment();
};
class QualitySelectFragment : public brls::Box {
public:
    QualitySelectFragment();
};
class SpeedSelectFragment : public brls::Box {
public:
    SpeedSelectFragment();
};
}
