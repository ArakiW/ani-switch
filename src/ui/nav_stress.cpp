// SPDX-License-Identifier: AGPL-3.0
//
// Eden/emulator navigation stress harness.
// Each cycle opens a battery of secondary pages then pops them back
// toward the Main root, logging stack depth so host scripts can detect
// push/pop leaks or crashes without controller input injection.

#include "ui/nav_stress.hpp"

#include "player/mpv_core.hpp"
#include "utils/activity_helper.hpp"

#include <borealis/core/actions.hpp>
#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/core/thread.hpp>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// startup.log is only written when ANISWITCH_SWITCH_DEBUG=ON
// (platform/switch/switch_wrapper.c startupLog). Call
// aniswitchStartupLog on all Switch builds so DEBUG NROs land
// STRESS: lines for the host parser; also always brls::Logger::info.
#if defined(__SWITCH__)
extern "C" void aniswitchStartupLog(const char*);
#endif

namespace aniswitch {
namespace {

std::atomic<bool> g_running{false};
std::atomic<int> g_fails{0};
std::atomic<int> g_peakDepth{0};

size_t stackDepth() {
    return brls::Application::getActivitiesStack().size();
}

void logf(const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    brls::Logger::info("{}", buf);
#if defined(__SWITCH__)
    aniswitchStartupLog(buf);
#endif
}

// One open → wait → optional B-action → pop step.
struct Step {
    const char* name;
    std::function<void()> open;
    bool tryBackAction = true;
};

std::vector<Step> makeBattery() {
    std::vector<Step> steps;
    steps.push_back({"settings", [] { Intent::openSettings(); }});
    steps.push_back({"local_video", [] { Intent::openLocalVideo(); }});
    steps.push_back({"search", [] { Intent::openSearch(); }});
    steps.push_back({"collection", [] { Intent::openMyCollection(); }});
    steps.push_back({"history", [] { Intent::openHistory(); }});
    steps.push_back({"login", [] { Intent::openLogin(); }});
    steps.push_back({"subject_demo", [] { Intent::openSubject(-1); }});
    steps.push_back({"email_login", [] { Intent::openEmailLoginStart(); }});
    steps.push_back({"bangumi_sync", [] { Intent::openBangumiSync(); }});
    steps.push_back({"theme_preview", [] { Intent::openThemePreview(); }});
    steps.push_back({"log", [] { Intent::openLog(); }});
    steps.push_back({"episode_list", [] { Intent::openEpisodeList(-1); }});
    steps.push_back({"person_demo", [] { Intent::openPerson(1); }});
    steps.push_back({"source_picker_demo",
                     [] { Intent::openSourcePicker(-1, -1, "stress", false); }});
    // ONBOARDING EXCLUDED from default battery:
    // Intent::openOnboarding() only pushActivity; ProgramConfig is mutated
    // only if the user COMPLETES the flow (markFirstRunDone) or Settings
    // "重新走引导" (resetFirstRun + openOnboarding). Completing also
    // popActivity + Intent::openMain() which pushes a SECOND Main and
    // breaks "depth back to 1". v17.3-17.5 also crashed on first-frame
    // layout. Prefer exclude; do not open onboarding in the 50-loop.
    // openPlayer excluded: mpv init risk (startup.8 / v20.4).
    // openSourcePicker autoPickFirst=true excluded: chains into player.
    return steps;
}

// Invoke the first registered B (back) action on the current activity
// contentView — multi-key "返回" without HID. borealis Action has no
// fire(); call getActionListener()(view) like Application::handleAction
// (application.cpp ~796). Activity::registerAction lands on contentView.
bool tryInvokeBackAction() {
    auto stack = brls::Application::getActivitiesStack();
    if (stack.size() <= 1) return false;
    brls::Activity* act = stack.back();
    if (!act) return false;
    brls::View* view = act->getContentView();
    if (!view) return false;
    for (const auto& action : view->getActions()) {
        if (!action) continue;
        if (action->getType() != brls::ACTION_GAMEPAD) continue;
        if (action->getButton() != brls::BUTTON_B) continue;
        if (!action->isAvailable() || action->isHidden()) continue;
        auto listener = action->getActionListener();
        if (!listener) continue;
        listener(view);
        logf("STRESS: key B fired on depth=%d", static_cast<int>(stackDepth()));
        return true;
    }
    return false;
}

// Every 10 cycles on Main (depth==1): fire LB/RB home-tab cycle actions.
void maybeFireMainTabActions(int cycle) {
    if (cycle % 10 != 0) return;
    if (stackDepth() != 1) return;
    auto stack = brls::Application::getActivitiesStack();
    if (stack.empty() || !stack.back()) return;
    brls::View* view = stack.back()->getContentView();
    if (!view) return;
    int fired = 0;
    for (const auto& action : view->getActions()) {
        if (!action) continue;
        if (action->getType() != brls::ACTION_GAMEPAD) continue;
        const int btn = action->getButton();
        if (btn != brls::BUTTON_LB && btn != brls::BUTTON_RB) continue;
        if (!action->isAvailable() || action->isHidden()) continue;
        auto listener = action->getActionListener();
        if (!listener) continue;
        listener(view);
        ++fired;
    }
    if (fired > 0)
        logf("STRESS: main multi-key LB/RB fired n=%d cycle=%d", fired, cycle);
}

// After open: require stack growth (push is synchronous in pushActivity).
void checkOpenGrowth(const char* stepName, size_t before, size_t afterOpen) {
    if (afterOpen <= before) {
        logf("STRESS: FAIL %s push did not grow depth=%d->%d", stepName,
             static_cast<int>(before), static_cast<int>(afterOpen));
        g_fails++;
    }
}

// popActivity erases the stack entry only after the ~200ms hide
// animation (application.cpp ~1053). Callers must delay again before
// asserting depth.
//   now > before → popActivity (B-equivalent if B handler didn't)
//   now == before → already returned (B-action pop) — OK
//   now < before → unexpected shrink → FAIL
void popIfNeeded(const char* stepName, size_t depthBefore) {
    const size_t now = stackDepth();
    if (now > depthBefore) {
        brls::Application::popActivity();
        logf("STRESS: pop %s depth %d->pending", stepName,
             static_cast<int>(now));
    } else if (now < depthBefore) {
        logf("STRESS: FAIL %s depth shrank %d->%d (unexpected pop)",
             stepName, static_cast<int>(depthBefore), static_cast<int>(now));
        g_fails++;
    } else {
        logf("STRESS: settle %s depth=%d ok", stepName,
             static_cast<int>(now));
    }
}

void runCycle(int cycle, int totalCycles, const std::vector<Step>& battery);

void finishStress(int totalCycles) {
    const int fails = g_fails.load();
    const int peak = g_peakDepth.load();
    const size_t endDepth = stackDepth();
    if (endDepth != 1) {
        logf("STRESS: WARN endDepth=%d expected 1", static_cast<int>(endDepth));
        // Drain extras.
        while (stackDepth() > 1) brls::Application::popActivity();
    }
    if (fails == 0 && endDepth == 1) {
        logf("STRESS: DONE cycles=%d fails=0 peakDepth=%d", totalCycles, peak);
    } else {
        logf("STRESS: FAIL cycles=%d fails=%d peakDepth=%d endDepth=%d",
             totalCycles, fails, peak, static_cast<int>(endDepth));
    }
    g_running = false;
}

void runStep(int cycle, int totalCycles, const std::vector<Step>& battery,
             size_t stepIndex) {
    if (!g_running.load()) return;
    if (stepIndex >= battery.size()) {
        // End of battery — next cycle.
        logf("STRESS: cycle %d/%d complete depth=%d fails=%d", cycle,
             totalCycles, static_cast<int>(stackDepth()), g_fails.load());
        if (cycle >= totalCycles) {
            finishStress(totalCycles);
            return;
        }
        // Small pause between cycles so UI can paint.
        brls::delay(300, [cycle, totalCycles, battery]() {
            runCycle(cycle + 1, totalCycles, battery);
        });
        return;
    }

    const Step& step = battery[stepIndex];
    const size_t before = stackDepth();
    if (static_cast<int>(before) > g_peakDepth.load())
        g_peakDepth.store(static_cast<int>(before));

    logf("STRESS: open %s c=%d s=%zu depth=%d", step.name, cycle, stepIndex,
         static_cast<int>(before));
    try {
        step.open();
    } catch (const std::exception& e) {
        logf("STRESS: FAIL %s open exception %s", step.name, e.what());
        g_fails++;
    } catch (...) {
        logf("STRESS: FAIL %s open unknown exception", step.name);
        g_fails++;
    }

    const size_t afterOpen = stackDepth();
    if (static_cast<int>(afterOpen) > g_peakDepth.load())
        g_peakDepth.store(static_cast<int>(afterOpen));
    logf("STRESS: opened %s depth=%d", step.name, static_cast<int>(afterOpen));
    checkOpenGrowth(step.name, before, afterOpen);

    // Delay chain (NOT a blocking loop — UI must keep rendering):
    //   open → 350ms settle → optional B action → 280ms (B-pop anim)
    //   → popIfNeeded → 280ms (pop anim) → verify → 180ms → next step.
    // pushActivity pushes the stack synchronously; popActivity erases
    // only after ~200ms hide anim (application.cpp), so verify must
    // sit behind another delay after any pop call.
    brls::delay(350, [step, cycle, totalCycles, battery, stepIndex, before]() {
        if (!g_running.load()) return;
        if (step.tryBackAction) {
            try {
                tryInvokeBackAction();
            } catch (...) {
                logf("STRESS: FAIL %s B-action exception", step.name);
                g_fails++;
            }
        }
        brls::delay(280, [step, cycle, totalCycles, battery, stepIndex,
                          before]() {
            if (!g_running.load()) return;
            try {
                popIfNeeded(step.name, before);
            } catch (...) {
                logf("STRESS: FAIL %s pop exception", step.name);
                g_fails++;
            }
            // Wait for pop hide-animation before asserting depth.
            brls::delay(280, [step, cycle, totalCycles, battery, stepIndex,
                              before]() {
                if (!g_running.load()) return;
                size_t now = stackDepth();
                int guard = 0;
                while (now > before && guard++ < 4) {
                    brls::Application::popActivity();
                    now = stackDepth();
                }
                if (now != before) {
                    // One more short wait if a pop was just issued.
                    if (now > before) {
                        brls::delay(250, [step, cycle, totalCycles, battery,
                                          stepIndex, before]() {
                            if (!g_running.load()) return;
                            const size_t late = stackDepth();
                            if (late != before) {
                                logf("STRESS: FAIL %s settle depth=%d expected=%d",
                                     step.name, static_cast<int>(late),
                                     static_cast<int>(before));
                                g_fails++;
                                while (stackDepth() > 1)
                                    brls::Application::popActivity();
                            }
                            brls::delay(180, [cycle, totalCycles, battery,
                                             stepIndex]() {
                                runStep(cycle, totalCycles, battery,
                                        stepIndex + 1);
                            });
                        });
                        return;
                    }
                    logf("STRESS: FAIL %s settle depth=%d expected=%d",
                         step.name, static_cast<int>(now),
                         static_cast<int>(before));
                    g_fails++;
                    while (stackDepth() > 1) brls::Application::popActivity();
                }
                brls::delay(180, [cycle, totalCycles, battery, stepIndex]() {
                    runStep(cycle, totalCycles, battery, stepIndex + 1);
                });
            });
        });
    });
}

void runCycle(int cycle, int totalCycles, const std::vector<Step>& battery) {
    if (!g_running.load()) return;
    logf("STRESS: BEGIN cycle %d/%d depth=%d", cycle, totalCycles,
         static_cast<int>(stackDepth()));
    if (stackDepth() != 1) {
        logf("STRESS: WARN cycle start depth=%d", static_cast<int>(stackDepth()));
        while (stackDepth() > 1) brls::Application::popActivity();
    }
    maybeFireMainTabActions(cycle);
    runStep(cycle, totalCycles, battery, 0);
}

}  // namespace

bool navStressRunning() { return g_running.load(); }

void startPlayTest(int32_t episodeId, int32_t subjectId) {
    logf("PLAYTEST: START ep=%d sid=%d", episodeId, subjectId);
    Intent::openSourcePicker(episodeId, subjectId, "playtest", true);

    auto seeks = std::make_shared<int>(0);
    auto fails = std::make_shared<int>(0);
    auto ep = std::make_shared<int32_t>(episodeId);
    auto started = std::make_shared<bool>(false);

    struct Op { int64_t rel; const char* name; };
    static const Op kOps[] = {
        {15, "ff15"}, {30, "ff30"}, {-20, "rw20"},
        {-40, "rw40"}, {25, "ff25"}, {10, "ff10"},
    };

    auto runSeeks = std::make_shared<std::function<void()>>();
    *runSeeks = [runSeeks, seeks, fails, ep]() {
        auto step = std::make_shared<size_t>(0);
        auto next = std::make_shared<std::function<void()>>();
        *next = [next, step, seeks, fails, ep]() {
            auto& m = MPVCore::instance();
            if (*step >= sizeof(kOps) / sizeof(kOps[0])) {
                logf("PLAYTEST: DONE ep=%d seeks=%d time=%.2f dur=%lld playing=%d stopped=%d",
                     (int)*ep, *seeks, m.playback_time,
                     (long long)m.duration, m.video_playing ? 1 : 0,
                     m.video_stopped ? 1 : 0);
                return;
            }
            const Op& op = kOps[*step];
            const double before = m.playback_time;
            m.resume();
            m.seekRelative(op.rel);
            (*seeks)++;
            logf("PLAYTEST: seek %s rel=%lld before=%.2f path=%s", op.name,
                 (long long)op.rel, before, m.filepath.c_str());
            (*step)++;
            brls::delay(2000, [next, op, before, fails]() {
                auto& mm = MPVCore::instance();
                const double after = mm.playback_time;
                logf("PLAYTEST: seek %s after=%.2f dur=%lld playing=%d stopped=%d",
                     op.name, after, (long long)mm.duration,
                     mm.video_playing ? 1 : 0, mm.video_stopped ? 1 : 0);
                if (after + 0.01 < before && op.rel > 0) {
                    (*fails)++;
                    logf("PLAYTEST: FAIL seek %s did not advance", op.name);
                }
                (*next)();
            });
        };
        (*next)();
    };

    auto poll = std::make_shared<std::function<void(int)>>();
    *poll = [poll, seeks, fails, ep, started, runSeeks](int attempt) {
        if (*started) return;
        auto& mpv = MPVCore::instance();
        const double t = mpv.playback_time;
        const int64_t d = mpv.duration;
        const bool stopped = mpv.video_stopped;
        const bool hasPath = !mpv.filepath.empty();
        if (attempt % 5 == 0 || attempt < 3) {
            logf("PLAYTEST: wait t=%d time=%.2f dur=%lld stopped=%d playing=%d path=%s",
                 attempt, t, (long long)d, stopped ? 1 : 0,
                 mpv.video_playing ? 1 : 0, mpv.filepath.c_str());
        }
        if (attempt > 120) {
            logf("PLAYTEST: FAIL ep=%d timeout time=%.2f dur=%lld stopped=%d",
                 (int)*ep, t, (long long)d, stopped ? 1 : 0);
            return;
        }
        // Kick decoder once file is on ani:// or local path.
        if (hasPath && attempt >= 3) {
            mpv.resume();
        }
        const bool live = !stopped && (t > 0.2 || d > 0 || mpv.video_playing);
        const bool force = hasPath && attempt >= 20 && !*started;
        if (live || force) {
            *started = true;
            logf("PLAYTEST: begin seeks time=%.2f dur=%lld live=%d force=%d",
                 t, (long long)d, live ? 1 : 0, force ? 1 : 0);
            (*runSeeks)();
            return;
        }
        brls::delay(1000, [poll, attempt]() { (*poll)(attempt + 1); });
    };
    // Give seamless buffer + first decode a few seconds.
    brls::delay(8000, [poll]() { (*poll)(0); });
}

void startNavStress(int cycles) {
    if (cycles <= 0) cycles = 50;
    if (cycles > 200) cycles = 200;  // emulator time budget
    bool expected = false;
    if (!g_running.compare_exchange_strong(expected, true)) {
        logf("STRESS: already running, ignore start cycles=%d", cycles);
        return;
    }
    g_fails = 0;
    g_peakDepth = static_cast<int>(stackDepth());
    logf("STRESS: START cycles=%d battery=14 (no player/onboarding)",
         cycles);
    auto battery = makeBattery();
    // Capture battery by value in delays via shared_ptr to avoid copies
    // of std::function chains exploding; vector is small enough.
    brls::delay(200, [cycles, battery]() { runCycle(1, cycles, battery); });
}

}  // namespace aniswitch
