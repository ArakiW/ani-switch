// SPDX-License-Identifier: AGPL-3.0
// ani-switch — HOME button LED status indicator.
//
// Uses libnx `hidsysSetNotificationLedPattern` (7.0.0+) on the
// console HOME button. Pad selection (per field directive):
//   1) hidsysGetUniquePadsFromNpad(HidNpadIdType_Handheld)  — TsVitch path
//   2) hidsysGetUniquePadIds() first entry
//   3) unique_pad_id = 0 (console default used by many homebrew)
//
// Never crash on service miss (applet / restricted mode): log and return.
#include "utils/home_led.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>

#if defined(__SWITCH__)
#include <switch.h>
#endif

#if defined(__SWITCH__)
extern "C" void aniswitchStartupLog(const char* message);
#else
// Host / test builds: no startup.log, no borealis dependency required.
static inline void aniswitchStartupLog(const char* message) {
    if (message) std::fprintf(stderr, "[LED] %s\n", message);
}
#endif

namespace aniswitch {
namespace {

constexpr uint64_t kDebounceMs = 400;

HomeLedStatus g_status     = HomeLedStatus::Off;
uint64_t      g_lastWriteMs = 0;
bool          g_hidsysReady = false;
bool          g_hidsysTried = false;
// v22.2: LED is playback-scoped. App-open / home HTTP must not blink.
bool          g_playbackScope = false;

uint64_t nowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
            .count());
}

void ledLog(const char* msg) {
    // Device QA: every transition lands in startup.log as `LED: ...`.
    aniswitchStartupLog(msg);
}

#if defined(__SWITCH__)

void fillLoadingBlink(HidsysNotificationLedPattern* p) {
    // Semantic "yellow blink": instant on/off, ~2.5 Hz.
    // base = 0x4 → 4 * 12.5 ms = 50 ms; finalStep 0x4 → 200 ms/step.
    std::memset(p, 0, sizeof(*p));
    p->baseMiniCycleDuration = 0x4;
    p->totalMiniCycles       = 0x1;  // 2 mini-cycles (field = count-1)
    p->totalFullCycles       = 0x0;  // forever
    p->startIntensity        = 0x0;
    p->miniCycles[0].ledIntensity      = 0xF;
    p->miniCycles[0].transitionSteps   = 0x0;  // instant (not fade)
    p->miniCycles[0].finalStepDuration = 0x4;  // 4 * 50ms = 200ms on
    p->miniCycles[1].ledIntensity      = 0x0;
    p->miniCycles[1].transitionSteps   = 0x0;
    p->miniCycles[1].finalStepDuration = 0x4;  // 200ms off
}

void fillOkSolid(HidsysNotificationLedPattern* p) {
    // Semantic "green": solid 100% intensity.
    // switchbrew: baseMiniCycleDuration=0 + totalFullCycles=0 → LED stays
    // on at startIntensity after a single 12.5ms step.
    std::memset(p, 0, sizeof(*p));
    p->baseMiniCycleDuration = 0x0;
    p->totalMiniCycles       = 0x0;
    p->totalFullCycles       = 0x0;
    p->startIntensity        = 0xF;
    p->miniCycles[0].ledIntensity      = 0xF;
    p->miniCycles[0].transitionSteps   = 0x0;
    p->miniCycles[0].finalStepDuration = 0x0;
}

void fillErrorDoubleBlink(HidsysNotificationLedPattern* p) {
    // Semantic "red": distinctive double-pulse so it is not confused
    // with the steady loading blink.
    // on-short → off-short → on-short → off-long → repeat
    std::memset(p, 0, sizeof(*p));
    p->baseMiniCycleDuration = 0x4;  // 50ms
    p->totalMiniCycles       = 0x3;  // 4 mini-cycles
    p->totalFullCycles       = 0x0;  // forever
    p->startIntensity        = 0x0;
    p->miniCycles[0].ledIntensity      = 0xF;
    p->miniCycles[0].transitionSteps   = 0x0;
    p->miniCycles[0].finalStepDuration = 0x1;  // 50ms on
    p->miniCycles[1].ledIntensity      = 0x0;
    p->miniCycles[1].transitionSteps   = 0x0;
    p->miniCycles[1].finalStepDuration = 0x1;  // 50ms off
    p->miniCycles[2].ledIntensity      = 0xF;
    p->miniCycles[2].transitionSteps   = 0x0;
    p->miniCycles[2].finalStepDuration = 0x1;  // 50ms on
    p->miniCycles[3].ledIntensity      = 0x0;
    p->miniCycles[3].transitionSteps   = 0x0;
    p->miniCycles[3].finalStepDuration = 0x8;  // 400ms off
}

void fillClear(HidsysNotificationLedPattern* p) {
    std::memset(p, 0, sizeof(*p));  // all-zero = LED off
}

bool ensureHidsys() {
    if (g_hidsysTried) return g_hidsysReady;
    g_hidsysTried = true;
    Result rc = hidsysInitialize();
    if (R_FAILED(rc)) {
        char buf[80];
        std::snprintf(buf, sizeof(buf), "LED: hidsysInitialize fail rc=0x%08lx",
                      static_cast<unsigned long>(rc));
        ledLog(buf);
        g_hidsysReady = false;
        return false;
    }
    g_hidsysReady = true;
    ledLog("LED: hidsysInitialize ok");
    return true;
}

bool sendPattern(const HidsysNotificationLedPattern& pattern) {
    if (!ensureHidsys()) return false;

    HidsysUniquePadId pads[4];
    std::memset(pads, 0, sizeof(pads));
    s32 count = 0;

    // 1) Handheld pads (HOME button lives here in portable mode).
    Result rc = hidsysGetUniquePadsFromNpad(
        HidNpadIdType_Handheld, pads, 2, &count);
    bool any = false;
    if (R_SUCCEEDED(rc) && count > 0) {
        for (s32 i = 0; i < count && i < 2; ++i) {
            Result pr = hidsysSetNotificationLedPattern(&pattern, pads[i]);
            if (R_FAILED(pr)) {
                ledLog("LED: fail");
            } else {
                any = true;
            }
        }
        return any;
    }

    // 2) Any unique pad (docked Pro Controller, etc.).
    count = 0;
    std::memset(pads, 0, sizeof(pads));
    rc = hidsysGetUniquePadIds(pads, 2, &count);
    if (R_SUCCEEDED(rc) && count > 0) {
        for (s32 i = 0; i < count && i < 2; ++i) {
            Result pr = hidsysSetNotificationLedPattern(&pattern, pads[i]);
            if (R_FAILED(pr)) {
                ledLog("LED: fail");
            } else {
                any = true;
            }
        }
        return any;
    }

    // 3) Console default unique_pad_id.id = 0 (chief directive).
    HidsysUniquePadId zero;
    std::memset(&zero, 0, sizeof(zero));
    zero.id = 0;  // explicit: console HOME button
    Result pr = hidsysSetNotificationLedPattern(&pattern, zero);
    if (R_FAILED(pr)) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "LED: fail rc=0x%08lx",
                      static_cast<unsigned long>(pr));
        ledLog(buf);
        return false;
    }
    return true;
}

#endif  // __SWITCH__

const char* statusName(HomeLedStatus s) {
    switch (s) {
        case HomeLedStatus::Off:     return "off";
        case HomeLedStatus::Loading: return "loading";
        case HomeLedStatus::Ok:      return "ok";
        case HomeLedStatus::Error:   return "error";
    }
    return "?";
}

void applyStatus(HomeLedStatus next) {
    // Primary debounce: identical enum → no IPC, no log spam.
    if (next == g_status) {
        return;
    }
    // Secondary debounce: non-error transitions within 400ms of the last
    // hardware write are skipped so a burst of micro-requests cannot
    // thrash hidsys. Error always applies (must not be swallowed).
    const uint64_t now = nowMs();
    if (next != HomeLedStatus::Error && g_lastWriteMs != 0 &&
        (now - g_lastWriteMs) < kDebounceMs) {
        char buf[96];
        std::snprintf(buf, sizeof(buf),
                      "LED: debounce skip %s->%s (%llums)",
                      statusName(g_status), statusName(next),
                      static_cast<unsigned long long>(now - g_lastWriteMs));
        ledLog(buf);
        g_status = next;  // track logical state; hardware catches up later
        return;
    }

    g_status      = next;
    g_lastWriteMs = now;

#if defined(__SWITCH__)
    HidsysNotificationLedPattern pattern;
    switch (next) {
        case HomeLedStatus::Off:     fillClear(&pattern); break;
        case HomeLedStatus::Loading: fillLoadingBlink(&pattern); break;
        case HomeLedStatus::Ok:      fillOkSolid(&pattern); break;
        case HomeLedStatus::Error:   fillErrorDoubleBlink(&pattern); break;
    }
    const bool ok = sendPattern(pattern);
    char buf[80];
    std::snprintf(buf, sizeof(buf), "LED: %s %s", statusName(next),
                  ok ? "ok" : "fail");
    ledLog(buf);
#else
    char buf[64];
    std::snprintf(buf, sizeof(buf), "LED: %s (noop host)", statusName(next));
    ledLog(buf);
#endif
}

}  // namespace

void homeLedSetLoading() {
    if (!g_playbackScope) return;
    applyStatus(HomeLedStatus::Loading);
}
void homeLedSetOk() {
    if (!g_playbackScope) return;
    applyStatus(HomeLedStatus::Ok);
}
void homeLedSetError() {
    if (!g_playbackScope) return;
    applyStatus(HomeLedStatus::Error);
}
void homeLedClear()      { applyStatus(HomeLedStatus::Off); }

void homeLedSetPlaybackScope(bool enabled) {
    g_playbackScope = enabled;
    if (!enabled) applyStatus(HomeLedStatus::Off);
}
bool homeLedPlaybackScope() { return g_playbackScope; }

HomeLedStatus homeLedGetStatus() { return g_status; }

}  // namespace aniswitch
