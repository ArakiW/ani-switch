// SPDX-License-Identifier: AGPL-3.0
// ani-switch — HOME button LED status indicator (libnx hidsys).
#pragma once

namespace aniswitch {

// Product brief mapping (colors are SEMANTIC — see hardware note):
//   loading  → yellow blink
//   ok/connected → green solid
//   error/read-fail → red (double-pulse)
//
// HARDWARE NOTE (libnx 2.28 / devkitA64 20260219):
//   HidsysNotificationLedPattern has NO RGB / color fields. The HOME
//   LED is monochrome PWM intensity only. "Yellow / green / red" are
//   encoded as distinct blink patterns, not actual colors:
//     Loading → rapid on/off blink
//     Ok      → solid high intensity
//     Error   → double-pulse blink (on-off-on-long-off)
//   Structure fields actually present:
//     baseMiniCycleDuration  (u8, 0x0-0xF → 0 / 12.5ms-187.5ms steps)
//     totalMiniCycles        (u8, field = count-1, 0x0-0xF → 1-16)
//     totalFullCycles        (u8, 0x0 = repeat forever)
//     startIntensity         (u8, 0x0-0xF → 0%-100%)
//     miniCycles[16]         { ledIntensity, transitionSteps,
//                              finalStepDuration, pad }
//     unk_x44[2], pad_x46[2]
//   Only the low 4 bits of each used byte are consumed by the service.
//
// Off-Switch builds compile to no-ops. Every libnx Result is checked;
// failures log `LED: fail` via startupLog and never crash (applet-safe).
// Debounce: hardware is only written when the status enum changes, and
// identical status re-fires within ~400 ms are ignored (except Error,
// which always applies so a hard fail cannot be swallowed by flapping).

enum class HomeLedStatus {
    Off = 0,
    Loading,  // yellow ≈ blink
    Ok,       // green  ≈ solid
    Error,    // red    ≈ double-blink
};

void homeLedSetLoading();
void homeLedSetOk();
void homeLedSetError();
void homeLedClear();

// v22.2: LED only during playback loading — not on app boot / home HTTP.
// Default false (setStatus* are hardware no-ops). PlayerActivity / mpv
// enable around playback and clear on exit.
void homeLedSetPlaybackScope(bool enabled);
bool homeLedPlaybackScope();

// Logical status after debounce (not necessarily last hardware write).
HomeLedStatus homeLedGetStatus();

}  // namespace aniswitch
