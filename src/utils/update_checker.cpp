// SPDX-License-Identifier: AGPL-3.0
#include "utils/update_checker.hpp"
#include <borealis/core/logger.hpp>
#include <algorithm>
#include <cctype>
#include <cstdio>

#if ANISWITCH_HAS_CPR
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#endif

namespace aniswitch {

namespace {

// Split a version string into (core, preRelease) parts.
// core is the dotted integer sequence (e.g. [0,2,0] from
// "0.2.0" or "0.2.0-rc1").  preRelease is the tail after the
// first '-' (e.g. "rc1" or empty for a final release).
std::pair<std::vector<int>, std::string> splitVersion(const std::string& s) {
    std::pair<std::vector<int>, std::string> out;
    if (s.empty()) return out;
    size_t i = 0;
    if (s[0] == 'v' || s[0] == 'V') i = 1;
    // Find the first '-' (if any); everything after is the pre-release tail.
    size_t dash = std::string::npos;
    for (size_t k = i; k < s.size(); ++k) {
        if (s[k] == '-') { dash = k; break; }
    }
    const size_t coreEnd = (dash == std::string::npos) ? s.size() : dash;
    // Parse core as dotted integers.
    size_t p = i;
    while (p < coreEnd) {
        size_t end = coreEnd;
        for (size_t k = p; k < coreEnd; ++k) {
            if (s[k] == '.') { end = k; break; }
        }
        const std::string num = s.substr(p, end - p);
        int v = 0;
        bool any = false;
        for (char c : num) {
            if (c >= '0' && c <= '9') { v = v * 10 + (c - '0'); any = true; }
        }
        out.first.push_back(any ? v : 0);
        p = (end == coreEnd) ? coreEnd : end + 1;
    }
    if (dash != std::string::npos && dash + 1 < s.size()) {
        out.second = s.substr(dash + 1);
    }
    return out;
}

}  // namespace

int compareVersions(const std::string& a, const std::string& b) {
    if (a == b) return 0;
    auto pa = splitVersion(a);
    auto pb = splitVersion(b);
    // Compare core dotted numbers first.
    const size_t n = std::max(pa.first.size(), pb.first.size());
    if (pa.first.size() < n) pa.first.resize(n, 0);
    if (pb.first.size() < n) pb.first.resize(n, 0);
    for (size_t i = 0; i < n; ++i) {
        if (pa.first[i] != pb.first[i]) return pa.first[i] < pb.first[i] ? -1 : 1;
    }
    // Same core: a non-empty pre-release is LESS than an empty
    // pre-release.  When both are non-empty we fall back to a
    // string compare (good enough for rc1 vs rc2; the
    // user-facing comparison is the numeric core).
    if (pa.second.empty() && !pb.second.empty()) return 1;
    if (!pa.second.empty() && pb.second.empty()) return -1;
    if (pa.second == pb.second) return 0;
    return pa.second < pb.second ? -1 : 1;
}

namespace {
constexpr const char* kReleasesUrl =
    "https://api.github.com/repos/MiniMax139102/ani-switch/releases/latest";
}  // namespace

void checkForUpdate(const std::string& currentVersion, UpdateCb cb) {
    if (cb == nullptr) return;
    // v16.10: this used to `std::thread([](){ cpr::Get(...) }).detach()`
    // — the same Switch-newlib std::terminate landmine that broke
    // v11-v16.9's token refresh thread.  The function now runs
    // synchronously on the caller's thread.  The caller is expected
    // to invoke it from a place that can tolerate a ~5s blocking
    // HTTP request (the cpr timeout) and to marshal any UI work
    // from the callback onto the main thread via brls::sync.
    //
    // main.cpp currently disables the in-app update check at the
    // call site (see "main: update check skipped (v16.10)"); the
    // API surface is kept so a future brls::RepeatingTask-based
    // re-enable in v16.11 is a one-line uncomment.
    UpdateInfo info;
#if ANISWITCH_HAS_CPR
    try {
        cpr::Session s;
        s.SetUrl(cpr::Url{kReleasesUrl});
        s.SetTimeout(cpr::Timeout{5000});
        s.SetHeader(cpr::Header{{"User-Agent", "ani-switch"}});
        s.SetHeader(cpr::Header{{"Accept", "application/vnd.github+json"}});
        auto r = s.Get();
        if (r.status_code == 200 && !r.text.empty()) {
            auto j = nlohmann::json::parse(r.text);
            if (j.contains("tag_name") && j["tag_name"].is_string()) {
                info.version = j["tag_name"].get<std::string>();
            }
            if (j.contains("html_url") && j["html_url"].is_string()) {
                info.url = j["html_url"].get<std::string>();
            }
            if (j.contains("body") && j["body"].is_string()) {
                info.notes = j["body"].get<std::string>();
                if (info.notes.size() > 280) {
                    info.notes.resize(277);
                    info.notes += "...";
                }
            }
            info.newer = compareVersions(info.version, currentVersion) > 0
                      && !info.version.empty();
            brls::Logger::info("UpdateChecker: latest={} current={} newer={}",
                               info.version, currentVersion, info.newer);
        } else {
            brls::Logger::debug("UpdateChecker: GitHub returned {} (skipped)",
                                r.status_code);
        }
    } catch (const std::exception& e) {
        brls::Logger::warning("UpdateChecker: {}", e.what());
    } catch (...) {
        brls::Logger::warning("UpdateChecker: unknown exception in network fetch");
    }
#endif
    // The callback runs on the caller's thread; the user-side
    // is expected to marshal UI calls back to the main thread
    // via brls::sync, so a crash inside the callback (e.g.
    // touching brls::Application before the main loop is up)
    // can no longer take the NRO down.
    try {
        cb(info);
    } catch (...) {
        brls::Logger::warning("UpdateChecker: callback threw");
    }
}

}  // namespace aniswitch
