// SPDX-License-Identifier: AGPL-3.0
//
// v22 Eden stress: programmatic multi-page open/back cycles.
// HID injection into Eden's game window does not work, so navigation
// logic is exercised by Intent push + Application::popActivity chains
// driven from autotour mode "stress50".
#pragma once

#include <cstdint>

namespace aniswitch {

// Kick off asynchronous stress runs. Safe to call once after Main is up.
// cycles: how many full page-open/back loops (default 50).
void startNavStress(int cycles);

// True while a stress chain is in flight.
bool navStressRunning();

// Eden playtest: open online source picker → wait for playback →
// seek forward/backward several times via MPVCore. Logs PLAYTEST: lines.
void startPlayTest(int32_t episodeId, int32_t subjectId);

}  // namespace aniswitch
