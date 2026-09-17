// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
#include "utils/crash_helper.hpp"
#include <borealis/core/logger.hpp>
#include <csignal>
#include <cstdio>
#include <ctime>

#ifdef _WIN32
#  include <windows.h>
#endif

namespace aniswitch {

namespace {
    void onSignal(int sig) {
        const char* name = strsignal(sig);
        std::time_t now = std::time(nullptr);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", std::localtime(&now));
        char path[128];
        std::snprintf(path, sizeof(path), "crash-report-%s.txt", buf);
        std::FILE* f = std::fopen(path, "w");
        if (f) {
            std::fprintf(f, "ani-switch crash report\n");
            std::fprintf(f, "Signal: %d (%s)\n", sig, name ? name : "unknown");
            std::fprintf(f, "Time:   %s\n", buf);
            std::fclose(f);
        }
        // Re-raise with default handler so the OS generates a core dump.
        std::signal(sig, SIG_DFL);
        std::raise(sig);
    }
}  // namespace

void installCrashHandler() {
#ifdef __SWITCH__
    // Switch system applet handles crashes; nothing to do.
    return;
#else
    std::signal(SIGSEGV, onSignal);
    std::signal(SIGABRT, onSignal);
    std::signal(SIGILL,  onSignal);
    std::signal(SIGFPE,  onSignal);
    brls::Logger::info("installCrashHandler: signal handlers installed");
#endif
}

}  // namespace aniswitch
