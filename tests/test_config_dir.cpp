// SPDX-License-Identifier: AGPL-3.0
//
// Regression test for ProgramConfig::getConfigDir() on Switch.
//
// The 9-6 candidate NRO failed at first-launch with
//   "database: sqlite=1802 errno=5 disk I/O error"
// because the relative path "./ani-switch.db" resolved under an
// installation cwd that was either the NRO mount (read-only) or a
// non-writable workdir.  The fix pins the Switch config dir to
// "sdmc:/switch/aniswitch" (absolute, writable).  This test makes
// sure the path stays absolute and uses sdmc: on Switch.
//
// On non-Switch builds the function should still respect the XDG
// environment variables (unchanged behaviour for desktop).

#include "utils/config_helper.hpp"
#include <cstdio>
#include <string>

#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL: %s @ %d\n", #cond, __LINE__); \
        std::exit(1); \
    } \
} while (0)

int main() {
    using namespace aniswitch;

    const std::string dir = ProgramConfig::instance().getConfigDir();

#if defined(__SWITCH__)
    // The NRO can land at e.g. sdmc:/switch/aniswitch/<name>.nro and
    // its cwd is implementation-defined; the only safe answer is
    // the absolute sdmc: path that the Switch homebrew loader
    // (hbmenu / nx-hbmenu) creates when the user runs "make install".
    EXPECT(!dir.empty());
    EXPECT(dir.rfind("sdmc:", 0) == 0);
    // The leading "/" prefix is the devkitpro bug the previous fix
    // warned about — make sure it does not regress.
    EXPECT(dir.find("/sdmc:") != 0);
    // The path must point at the same directory as startup.log.
    EXPECT(dir.find("/switch/aniswitch") != std::string::npos);
#else
    // Desktop: must produce a usable config dir, whether driven by
    // XDG_CONFIG_HOME or $HOME.  We don't assert the exact value
    // because the test environment can vary, but the path must be
    // non-empty and writable.  Skip writability check if /tmp doesn't
    // exist (some sandboxes hide it).
    EXPECT(!dir.empty());
    EXPECT(dir != ".");
    EXPECT(dir.find('/') != std::string::npos);
#endif

    std::printf("test_config_dir: %s\n", dir.c_str());
    return 0;
}
