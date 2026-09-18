// SPDX-License-Identifier: AGPL-3.0
#include "core/http_source.hpp"

#include <nlohmann/json.hpp>
#include <borealis/core/logger.hpp>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <sstream>

#ifndef ANISWITCH_NO_CONFIG_HELPER
#include "utils/config_helper.hpp"
#endif

#if defined(__SWITCH__)
extern "C" void aniswitchStartupLog(const char* message);
#endif

namespace aniswitch {

namespace {

void resolveLog(const char* msg) {
    brls::Logger::info("{}", msg);
#if defined(__SWITCH__)
    aniswitchStartupLog(msg);
#endif
}

struct ManifestState {
    std::mutex mutex;
    std::string pathOverride;
    std::string pathCached;
    std::string lastError;
    nlohmann::json json;
    bool loaded = false;
    bool ok = false;
    size_t keyCount = 0;
};

ManifestState& state() {
    static ManifestState s;
    return s;
}

void stripBom(std::string& s) {
    if (s.size() >= 3 &&
        static_cast<unsigned char>(s[0]) == 0xEF &&
        static_cast<unsigned char>(s[1]) == 0xBB &&
        static_cast<unsigned char>(s[2]) == 0xBF) {
        s.erase(0, 3);
    }
}

bool loadLocked(ManifestState& st) {
    const std::string path =
        st.pathOverride.empty() ? HTTPSourceProvider::manifestPath()
                                : st.pathOverride;
    st.pathCached = path;
    st.json = nlohmann::json{};
    st.ok = false;
    st.keyCount = 0;
    st.lastError.clear();

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        st.lastError = "missing";
        st.loaded = true;
        char b[200];
        snprintf(b, sizeof(b), "RESOLVE: http-sources missing path=%s", path.c_str());
        resolveLog(b);
        return false;
    }
    std::string raw((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());
    stripBom(raw);
    if (raw.empty()) {
        st.lastError = "empty file";
        st.loaded = true;
        char b[200];
        snprintf(b, sizeof(b), "RESOLVE: http-sources empty path=%s", path.c_str());
        resolveLog(b);
        return false;
    }
    try {
        auto parsed = nlohmann::json::parse(raw);
        if (!parsed.is_object()) {
            st.lastError = "root is not object";
            st.loaded = true;
            char b[200];
            snprintf(b, sizeof(b), "RESOLVE: http-sources bad-root path=%s",
                     path.c_str());
            resolveLog(b);
            return false;
        }
        st.json = std::move(parsed);
        st.keyCount = st.json.size();
        st.ok = true;
        st.loaded = true;
        char b[200];
        snprintf(b, sizeof(b), "RESOLVE: http-sources ok path=%s keys=%zu",
                 path.c_str(), st.keyCount);
        resolveLog(b);
        return true;
    } catch (const std::exception& e) {
        st.lastError = e.what();
        st.loaded = true;
        char b[240];
        snprintf(b, sizeof(b), "RESOLVE: http-sources parse-fail path=%s err=%s",
                 path.c_str(), e.what());
        resolveLog(b);
        return false;
    }
}

void ensureLoadedLocked(ManifestState& st) {
    if (!st.loaded || !st.ok) {
        // Reload when never loaded OR last load failed (SD mount / file
        // may have appeared after first attempt).
        loadLocked(st);
    }
}

bool urlLooksHttp(const std::string& u) {
    return u.rfind("http://", 0) == 0 || u.rfind("https://", 0) == 0;
}

void appendKey(const nlohmann::json& manifest, int32_t key,
               std::vector<VideoSource>& out) {
    const auto k = std::to_string(key);
    if (!manifest.contains(k)) return;
    const auto& arr = manifest.at(k);
    if (!arr.is_array()) return;
    for (const auto& entry : arr) {
        try {
            if (!entry.is_object() || !entry.contains("url")) continue;
            VideoSource source;
            source.kind = SourceKind::HTTP;
            source.url = entry.at("url").get<std::string>();
            source.label = entry.value("label", "sources.json");
            source.priority = entry.value("priority", 0);
            if (!urlLooksHttp(source.url)) {
                char b[160];
                snprintf(b, sizeof(b),
                         "RESOLVE: http-sources skip non-http key=%d", key);
                resolveLog(b);
                continue;
            }
            out.push_back(std::move(source));
        } catch (...) {
            // Skip malformed entry; keep the rest of the array.
        }
    }
}

}  // namespace

std::string HTTPSourceProvider::manifestPath() {
#ifndef ANISWITCH_NO_CONFIG_HELPER
    return ProgramConfig::instance().getConfigDir() + "/sources.json";
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) return std::string(xdg) + "/ani-switch/sources.json";
    const char* home = std::getenv("HOME");
    if (home && *home) return std::string(home) + "/.config/ani-switch/sources.json";
    return "./config/sources.json";
#endif
}

void HTTPSourceProvider::setManifestPathOverride(const std::string& path) {
    std::lock_guard<std::mutex> lock(state().mutex);
    state().pathOverride = path;
    state().loaded = false;
    state().ok = false;
    state().json = nlohmann::json{};
    state().keyCount = 0;
    state().lastError.clear();
    state().pathCached.clear();
}

void HTTPSourceProvider::resetManifestCache() {
    std::lock_guard<std::mutex> lock(state().mutex);
    state().loaded = false;
    state().ok = false;
    state().json = nlohmann::json{};
    state().keyCount = 0;
    state().lastError.clear();
}

bool HTTPSourceProvider::manifestOk() {
    std::lock_guard<std::mutex> lock(state().mutex);
    ensureLoadedLocked(state());
    return state().ok;
}

size_t HTTPSourceProvider::manifestKeyCount() {
    std::lock_guard<std::mutex> lock(state().mutex);
    ensureLoadedLocked(state());
    return state().keyCount;
}

const std::string& HTTPSourceProvider::manifestPathCached() {
    std::lock_guard<std::mutex> lock(state().mutex);
    ensureLoadedLocked(state());
    return state().pathCached;
}

const std::string& HTTPSourceProvider::lastError() {
    std::lock_guard<std::mutex> lock(state().mutex);
    ensureLoadedLocked(state());
    return state().lastError;
}

std::vector<VideoSource> HTTPSourceProvider::lookup(const std::vector<int32_t>& keys) {
    std::vector<VideoSource> sources;
    std::string path;
    std::string err;
    bool ok = false;
    size_t nkeys = 0;
    int32_t hitKey = 0;
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        auto& st = state();
        ensureLoadedLocked(st);
        path = st.pathCached;
        err = st.lastError;
        ok = st.ok;
        nkeys = st.keyCount;
        if (ok) {
            for (int32_t key : keys) {
                if (key == 0) continue;
                const size_t before = sources.size();
                appendKey(st.json, key, sources);
                if (sources.size() > before) {
                    hitKey = key;
                    break;
                }
            }
        }
    }
    char b[240];
    if (hitKey != 0) {
        snprintf(b, sizeof(b),
                 "RESOLVE: http-sources hit key=%d n=%zu path=%s", hitKey,
                 sources.size(), path.c_str());
    } else if (!ok) {
        snprintf(b, sizeof(b),
                 "RESOLVE: http-sources miss n=0 path=%s err=%s keysWanted=%zu",
                 path.c_str(), err.empty() ? "?" : err.c_str(), keys.size());
    } else {
        snprintf(b, sizeof(b),
                 "RESOLVE: http-sources miss n=0 path=%s fileKeys=%zu wanted=%d",
                 path.c_str(), nkeys, keys.empty() ? 0 : keys.front());
    }
    resolveLog(b);
    return sources;
}

void HTTPSourceProvider::enumerate(int32_t episodeId, SourceListCb callback,
                                   std::function<void(const std::string&, int)> error) {
    (void)error;  // soft-fail: never abort SourceManager
    auto sources = lookup({episodeId});
    if (callback) callback(std::move(sources));
}

}  // namespace aniswitch
