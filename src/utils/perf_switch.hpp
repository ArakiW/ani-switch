// SPDX-License-Identifier: AGPL-3.0
//
// v22.1 Switch performance + telemetry helper.
//
// Purpose: give startup.log enough PERF: breadcrumbs to diagnose
// player-page hang / crash on real device (multi-core / memory / GPU).
//
// Policy (chief directive 2026-09-18):
//   * NEVER call ApmCpuBoostMode_FastLoad during playback or network
//     loading. libnx documents FastLoad as "Boost CPU. Additionally,
//     throttle GPU to minimum" — fatal for deko3d + mpv render path.
//   * Short load windows may explicitly request ApmCpuBoostMode_Normal
//     only. Long-term FastLoad is forbidden.
//   * GPU: no public homebrew occupancy/perf API. Log `PERF: gpu=n/a`
//     and do not invent symbols.
//
// Available libnx APIs (verified in devkitpro/devkita64:20260219):
//   appletGetPerformanceMode()            -> ApmPerformanceMode
//   appletSetCpuBoostMode(mode)           -> only Normal(0)/FastLoad(1)
//   apmGetPerformanceMode / apmSet/GetPerformanceConfiguration
//   svcGetInfo(InfoType_CoreMask / UsedMemorySize / TotalMemorySize /
//              FreeThreadCount)
//   svcGetSystemInfo(SystemInfoType_TotalPhysicalMemorySize /
//                    SystemInfoType_UsedPhysicalMemorySize)
//   NO armGetCoreCount in libnx — derive core count from CoreMask bits.
//   NO public GPU utilisation API — log gpu=n/a.
#pragma once

namespace aniswitch {
namespace perf {

// One-shot init (safe to call multiple times). Logs current
// performance mode, core mask, process/system memory. Does not
// change any performance mode.
void init();

// Log one `PERF: <tag> ...` line with cores / mem / perf-mode / gpu=n/a.
// Cheap enough for player open / close and download milestones.
void sample(const char* tag);

// Short load-window helper (e.g. just before download-then-play).
// Explicitly requests CpuBoostMode Normal (NOT FastLoad) and samples.
void beginLoadWindow(const char* tag);

// Pair with beginLoadWindow. Re-asserts Normal and samples.
void endLoadWindow(const char* tag);

}  // namespace perf
}  // namespace aniswitch
