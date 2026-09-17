// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
#pragma once

namespace aniswitch {

// Install a signal handler that writes a minimal crash report to the
// platform's log directory. On Switch, this is a no-op (the system
// applet handles crashes). On Linux/macOS/Windows it writes
// crash-report-<timestamp>.txt.
void installCrashHandler();

}  // namespace aniswitch
