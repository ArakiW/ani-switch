// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
//
// Version metadata + GitHub release check.
// Renamed from wiliwili::APPVersion to aniswitch::APPVersion; checks
// the ani-switch repo instead of the wiliwili one.

#pragma once

#include <string>
#include <borealis/core/singleton.hpp>

namespace aniswitch {

class APPVersion : public brls::Singleton<APPVersion> {
    inline static std::string RELEASE_API =
        "https://api.github.com/repos/MiniMax139102/ani-switch/releases/latest";

public:
    int major, minor, revision;

    APPVersion();

    std::string getVersionStr();
    std::string getPlatform();

    static std::string getPackageName();

    bool needUpdate(std::string latestVersion);

    void checkUpdate(int delay = 2000, bool showUpToDateDialog = false);

    std::string git_commit, git_tag;
};

}  // namespace aniswitch
