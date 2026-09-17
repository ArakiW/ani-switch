// SPDX-License-Identifier: AGPL-3.0
//
// Smoke test for UpdateChecker's pure-function bit (semver
// comparison).  The actual GitHub fetch is exercised only on
// real hardware / in the smoke test, since the test target
// deliberately doesn't link cpr.

#include "utils/update_checker.hpp"
#include <cstdio>

int main() {
    using namespace aniswitch;

    auto expect = [](int got, int want, const char* what) {
        if (got != want) {
            std::fprintf(stderr, "FAIL: %s: got %d want %d\n", what, got, want);
            return 1;
        }
        return 0;
    };

    int r = 0;
    r |= expect(compareVersions("0.1.0", "0.1.0"), 0, "equal");
    r |= expect(compareVersions("0.1.0", "0.1.1"), -1, "patch");
    r |= expect(compareVersions("0.1.1", "0.1.0"),  1, "patch reverse");
    r |= expect(compareVersions("0.2.0", "0.1.9"),  1, "minor");
    r |= expect(compareVersions("1.0.0", "0.99.99"), 1, "major");
    r |= expect(compareVersions("v0.2.0", "0.1.0"), 1, "v-prefix");
    r |= expect(compareVersions("V1.0.0", "v1.0.0"), 0, "v-prefix both");
    r |= expect(compareVersions("0.2.0", "0.2.0-rc1"), 1, "release > rc");
    r |= expect(compareVersions("0.2.0-rc1", "0.2.0-rc1"), 0, "rc equal");
    r |= expect(compareVersions("0.2.0-rc2", "0.2.0-rc1"), 1, "rc monotonic");
    r |= expect(compareVersions("0.1", "0.1.0"), 0, "missing patch");
    r |= expect(compareVersions("0.1.0.0", "0.1"), 0, "extra build");

    if (r) return 1;
    std::puts("test_update_checker: OK");
    return 0;
}
