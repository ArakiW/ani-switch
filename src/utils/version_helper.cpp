// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)

#include "utils/version_helper.hpp"
#include <borealis/core/logger.hpp>
#include <fmt/format.h>

namespace aniswitch {

APPVersion::APPVersion() {
    // v20.11: proxy-first + Ani data plane + home IA rebuild.
    this->major    = 0;
    this->minor    = 21;
    this->revision = 3;
    this->git_commit = "dev";
    this->git_tag    = "v21.3";
}

std::string APPVersion::getVersionStr() {
    return fmt::format("{}.{}.{}", major, minor, revision);
}

std::string APPVersion::getPlatform() {
#ifdef __SWITCH__
    return "Switch";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#elif defined(_WIN32)
    return "Windows";
#else
    return "Unknown";
#endif
}

std::string APPVersion::getPackageName() {
    return "ani-switch";
}

bool APPVersion::needUpdate(std::string latestVersion) {
    // Strip leading 'v' or 'V'
    if (!latestVersion.empty() && (latestVersion[0] == 'v' || latestVersion[0] == 'V')) {
        latestVersion = latestVersion.substr(1);
    }
    // Format: "0.1.0"
    int lmaj = 0, lmin = 0, lrev = 0;
    if (sscanf(latestVersion.c_str(), "%d.%d.%d", &lmaj, &lmin, &lrev) != 3) {
        return false;
    }
    if (lmaj != major) return lmaj > major;
    if (lmin != minor) return lmin > minor;
    return lrev > revision;
}

void APPVersion::checkUpdate(int delay, bool showUpToDateDialog) {
    // Stub: would normally fire off a cpr::Get to RELEASE_API and compare versions.
    // We don't ship a settings UI in the initial release, so this is a no-op.
    brls::Logger::info("APPVersion::checkUpdate: skipped (delay={}, showUpToDate={})",
                       delay, showUpToDateDialog);
}

}  // namespace aniswitch
