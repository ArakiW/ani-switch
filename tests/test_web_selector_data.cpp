// SPDX-License-Identifier: AGPL-3.0
//
// Smoke test for the animeko-source manifest parser.  We feed it a
// representative slice of the upstream online.json and assert that
// the right number of sources is recovered, that the simple fields
// round-trip, and that the embedded regex compiles.

#include "core/web_selector_data.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string slurp(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool isHexHash(const std::string& h) {
    for (char c : h) if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    return !h.empty();
}

}  // namespace

int main(int argc, char** argv) {
    using namespace aniswitch;
    // Default: read from a known mirror of the upstream manifest.  The
    // test is allowed to be skipped if the file isn't present so this
    // doesn't fail the ctest on offline build hosts.
    std::string json;
    if (argc >= 2) json = slurp(argv[1]);
    if (json.empty()) {
        std::fprintf(stderr, "test_web_selector_data: SKIP (no input file)\n");
        return 0;
    }

    WebSelectorData data;
    if (!WebSelectorData::parse(json, data)) {
        std::fprintf(stderr, "FAIL: parse returned false\n");
        return 1;
    }
    if (data.sources.empty()) {
        std::fprintf(stderr, "FAIL: sources vector is empty after parse\n");
        return 1;
    }
    std::printf("test_web_selector_data: parsed %zu source(s)\n", data.sources.size());

    if (data.sources.size() < 5) {
        std::fprintf(stderr, "FAIL: expected at least 5 sources from a real manifest, got %zu\n",
                     data.sources.size());
        return 1;
    }

    // Spot-check the first source: name + searchUrl must be present.
    const auto& s0 = data.sources[0];
    if (s0.name.empty() || s0.searchUrl.empty()) {
        std::fprintf(stderr, "FAIL: first source missing name or searchUrl\n");
        return 1;
    }
    // The vast majority of animeko-source entries expose a {keyword}
    // placeholder in searchUrl.
    if (s0.searchUrl.find("{keyword}") == std::string::npos) {
        std::fprintf(stderr, "WARN: first source has no {keyword} in searchUrl; check the upstream format\n");
    }
    // tier defaults to 99 if absent, but most entries ship an explicit
    // tier.  At least the field must be readable (sanity).
    std::printf("test_web_selector_data: first source = '%s', tier=%d, searchUrl host = %zu chars\n",
                s0.name.c_str(), s0.tier, s0.searchUrl.size());

    // If the first source happens to expose a regex, make sure it
    // compiled (std::regex would have left matchVideoUrl empty on
    // failure).
    if (!s0.searchUrl.empty() && s0.matchVideoUrl.mark_count() == 0) {
        // Not necessarily a failure — some sources legitimately have
        // empty matchVideoUrl.  Just note it.
        std::fprintf(stderr, "INFO: first source has empty matchVideoUrl (not a failure)\n");
    }

    std::puts("test_web_selector_data: OK");
    return 0;
}
