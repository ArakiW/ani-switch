// SPDX-License-Identifier: AGPL-3.0
//
// In-app update check.  Polls the GitHub Releases API for the
// upstream ani-switch repository once per app run (cpr async,
// 5-second timeout) and surfaces a "newer version available"
// notification via brls::Application::notify.  Pure version-compare
// is in `compareVersions` so it can be unit-tested without cpr.

#pragma once

#include <functional>
#include <string>

namespace aniswitch {

struct UpdateInfo {
    std::string version;     // e.g. "0.2.0" (semver-ish, may have leading "v")
    std::string url;         // html_url of the release
    std::string notes;       // release notes (truncated)
    bool        newer = false;
};

// Returns 1 if a > b, -1 if a < b, 0 if equal.  "v" prefix is
// tolerated.  Trailing pre-release tags (-rc1, -beta) compare
// lower than the same version without (so 0.2.0 > 0.2.0-rc1).
int compareVersions(const std::string& a, const std::string& b);

using UpdateCb = std::function<void(const UpdateInfo&)>;

// Fire an async GitHub Releases check.  Always invokes `cb` on a
// worker thread (possibly with `newer = false` and an empty
// version on network / parse failure).  The check is best-effort
// and never blocks the caller.
void checkForUpdate(const std::string& currentVersion, UpdateCb cb);

}  // namespace aniswitch
