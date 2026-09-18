// SPDX-License-Identifier: AGPL-3.0
//
// v22.1 Switch performance + telemetry helper implementation.
#include "utils/perf_switch.hpp"

#include <borealis/core/logger.hpp>
#include <cstdint>
#include <cstdio>

#if defined(__SWITCH__)
#include <switch.h>
extern "C" void aniswitchStartupLog(const char* message);
#endif

namespace aniswitch {
namespace perf {

namespace {

#if defined(__SWITCH__)
bool g_inited = false;

const char* perfModeName(ApmPerformanceMode m) {
    switch (m) {
        case ApmPerformanceMode_Normal:  return "Normal";
        case ApmPerformanceMode_Boost:   return "Boost";
        case ApmPerformanceMode_Invalid:
        default:                         return "Invalid";
    }
}

// Popcount CoreMask into an approximate usable-core count.
// Applet/hbmenu NROs typically see a subset of the 4 A57 cores.
int coresFromMask(u64 mask) {
    int n = 0;
    while (mask) {
        n += static_cast<int>(mask & 1ull);
        mask >>= 1;
    }
    return n;
}

void perfLog(const char* msg) {
    aniswitchStartupLog(msg);
    brls::Logger::info("{}", msg);
}
#endif  // __SWITCH__

}  // namespace

void init() {
#if defined(__SWITCH__)
    if (g_inited) {
        sample("perf.init.repeat");
        return;
    }
    g_inited = true;

    // Document policy in startup.log so a future reader does not
    // "helpfully" flip FastLoad on while chasing CPU numbers.
    perfLog("PERF: policy no-FastLoad (GPU throttle); gpu API unavailable");

    const ApmPerformanceMode mode = appletGetPerformanceMode();
    {
        char b[160];
        snprintf(b, sizeof(b),
                 "PERF: init appletGetPerformanceMode=%d (%s)",
                 static_cast<int>(mode), perfModeName(mode));
        perfLog(b);
    }

    // Probe raw apm as well (used internally by applet). Failure is fine
    // in hbmenu applet mode — we only log.
    {
        ApmPerformanceMode raw = ApmPerformanceMode_Invalid;
        const Result rc = apmGetPerformanceMode(&raw);
        char b[160];
        snprintf(b, sizeof(b),
                 "PERF: init apmGetPerformanceMode rc=0x%08lx mode=%d (%s)",
                 static_cast<unsigned long>(rc), static_cast<int>(raw),
                 perfModeName(raw));
        perfLog(b);
    }

    sample("perf.init");
#endif
}

void sample(const char* tag) {
#if defined(__SWITCH__)
    const char* t = tag ? tag : "?";

    u64 coreMask = 0, usedMem = 0, totalMem = 0, freeThreads = 0;
    const Result rcCore = svcGetInfo(&coreMask, InfoType_CoreMask,
                                     CUR_PROCESS_HANDLE, 0);
    const Result rcUsed = svcGetInfo(&usedMem, InfoType_UsedMemorySize,
                                     CUR_PROCESS_HANDLE, 0);
    const Result rcTot  = svcGetInfo(&totalMem, InfoType_TotalMemorySize,
                                     CUR_PROCESS_HANDLE, 0);
    const Result rcThr  = svcGetInfo(&freeThreads, InfoType_FreeThreadCount,
                                     CUR_PROCESS_HANDLE, 0);

    u64 sysUsed = 0, sysTotal = 0;
    const Result rcSysUsed = svcGetSystemInfo(
        &sysUsed, SystemInfoType_UsedPhysicalMemorySize, 0, 0);
    const Result rcSysTot = svcGetSystemInfo(
        &sysTotal, SystemInfoType_TotalPhysicalMemorySize, 0, 0);

    const ApmPerformanceMode mode = appletGetPerformanceMode();
    const int cores = coresFromMask(coreMask);

    char b[320];
    snprintf(b, sizeof(b),
             "PERF: %s perfMode=%s cores=%d coreMask=0x%llx "
             "procMem=%llu/%llu sysMem=%llu/%llu freeThr=%llu "
             "gpu=n/a rc(c/u/t/th/su/st)=%08lx/%08lx/%08lx/%08lx/%08lx/%08lx",
             t,
             perfModeName(mode),
             cores,
             static_cast<unsigned long long>(coreMask),
             static_cast<unsigned long long>(usedMem),
             static_cast<unsigned long long>(totalMem),
             static_cast<unsigned long long>(sysUsed),
             static_cast<unsigned long long>(sysTotal),
             static_cast<unsigned long long>(freeThreads),
             static_cast<unsigned long>(rcCore),
             static_cast<unsigned long>(rcUsed),
             static_cast<unsigned long>(rcTot),
             static_cast<unsigned long>(rcThr),
             static_cast<unsigned long>(rcSysUsed),
             static_cast<unsigned long>(rcSysTot));
    perfLog(b);
#else
    (void)tag;
#endif
}

void beginLoadWindow(const char* tag) {
#if defined(__SWITCH__)
    const char* t = tag ? tag : "load";
    // Explicit Normal only. FastLoad is banned here: it throttles the
    // GPU to minimum and would sabotage deko3d/mpv during download.
    const Result rc =
        appletSetCpuBoostMode(ApmCpuBoostMode_Normal);
    {
        char b[160];
        snprintf(b, sizeof(b),
                 "PERF: %s beginLoadWindow CpuBoostMode=Normal rc=0x%08lx "
                 "(FastLoad forbidden)",
                 t, static_cast<unsigned long>(rc));
        perfLog(b);
    }
    sample(tag);
#else
    (void)tag;
#endif
}

void endLoadWindow(const char* tag) {
#if defined(__SWITCH__)
    const char* t = tag ? tag : "load";
    const Result rc = appletSetCpuBoostMode(ApmCpuBoostMode_Normal);
    {
        char b[128];
        snprintf(b, sizeof(b),
                 "PERF: %s endLoadWindow CpuBoostMode=Normal rc=0x%08lx",
                 t, static_cast<unsigned long>(rc));
        perfLog(b);
    }
    sample(tag);
#else
    (void)tag;
#endif
}

}  // namespace perf
}  // namespace aniswitch
