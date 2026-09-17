// SPDX-License-Identifier: AGPL-3.0
//
// Unit test for ImageLoader's pure-function bits (URL hashing +
// cachePathFor) without ever touching cpr.  The actual network
// fetch is exercised only on real hardware / in the smoke tests.
//
// The goal of this test is to catch two regressions that would
// otherwise be silent: empty / disabled cache dir mishandling and
// a non-deterministic hash (which would make the cache useless).

#include "utils/image_loader.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

int main() {
    using namespace aniswitch;

    // Default cache dir is non-empty.
    auto& loader = ImageLoader::instance();
    {
        const std::string def = loader.cachePathFor("https://example.com/foo.png");
        if (def.empty()) {
            std::fprintf(stderr, "FAIL: default cachePathFor should not be empty\n");
            return 1;
        }
        if (def.find("foo") != std::string::npos) {
            std::fprintf(stderr, "FAIL: cachePathFor should hash the URL, not embed it\n");
            return 1;
        }
    }

    // Two equal URLs produce the same path.
    const std::string a = loader.cachePathFor("https://x/y?z=1");
    const std::string b = loader.cachePathFor("https://x/y?z=1");
    if (a != b) {
        std::fprintf(stderr, "FAIL: cachePathFor is not deterministic for equal URLs\n");
        return 1;
    }

    // Two different URLs produce different paths.
    const std::string c = loader.cachePathFor("https://x/y?z=2");
    if (a == c) {
        std::fprintf(stderr, "FAIL: cachePathFor collides for distinct URLs\n");
        return 1;
    }

    // Empty URL returns empty.
    if (!loader.cachePathFor("").empty()) {
        std::fprintf(stderr, "FAIL: cachePathFor(\"\") should return empty\n");
        return 1;
    }

    // Disabling the cache dir makes cachePathFor return empty.
    loader.setCacheDir("");
    if (!loader.cachePathFor("https://x").empty()) {
        std::fprintf(stderr, "FAIL: cachePathFor should be empty when cache dir is cleared\n");
        return 1;
    }

    // With caching disabled, calling load() on an empty URL must
    // fire its callback immediately with "" (synchronously, in this
    // test thread).  This is the API contract for invalid input.
    bool fired = false;
    std::string got;
    loader.load("", [&fired, &got](const std::string& p) { fired = true; got = p; }, "test-tag");
    if (!fired || !got.empty()) {
        std::fprintf(stderr, "FAIL: load(\"\") should synchronously call cb with empty path\n");
        return 1;
    }

    // Reset the cache dir to a temp dir so this test doesn't pollute
    // the user's real cache.
    const fs::path tmp = fs::temp_directory_path() / "aniswitch-image-loader-test";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp, ec);
    loader.setCacheDir(tmp.string());

    std::puts("test_image_loader: OK");
    return 0;
}
