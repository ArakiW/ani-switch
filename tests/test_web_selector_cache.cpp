// SPDX-License-Identifier: AGPL-3.0
//
// Smoke test for the WebSelector HTML cache.  We point the cache at a
// temp directory, drop a few synthetic search/detail HTML files with
// the names the provider expects, then call readSearchHtml /
// readDetailHtml directly to confirm the lookup logic and the path
// sanitizer both work.
//
// We test the free functions in `web_selector_cache` rather than the
// WebSelectorProvider facade because the provider pulls in cpr/borealis
// (via web_selector_provider.cpp) which is not linkable in this test
// target.  The facade just forwards, so covering the underlying
// functions is enough.

#include "core/web_selector_cache.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

int expectEq(const std::string& got, const std::string& want, const char* what) {
    if (got != want) {
        std::fprintf(stderr, "FAIL: %s got '%s' want '%s'\n", what, got.c_str(), want.c_str());
        return 1;
    }
    return 0;
}

}  // namespace

int main() {
    using namespace aniswitch::cache;

    // Sanitizer smoke test.  We don't hardcode the exact output for
    // multi-byte CJK input (the bytes-per-char ratio depends on the
    // execution charset of the compiler), but we *do* confirm that
    // the output is purely [A-Za-z0-9._-] and the length is capped
    // at 64.
    {
        const std::string s = sanitizeForPath("酱紫社(修复)");
        for (char c : s) {
            const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                          || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
            if (!ok) {
                std::fprintf(stderr, "FAIL: sanitizer produced non-pure output: '%s'\n", s.c_str());
                return 1;
            }
        }
        if (s.size() > 64) {
            std::fprintf(stderr, "FAIL: sanitizer exceeded 64-char cap: %zu\n", s.size());
            return 1;
        }
        if (s.empty()) {
            std::fprintf(stderr, "FAIL: sanitizer produced empty output for non-empty input\n");
            return 1;
        }
    }
    {
        // ASCII + punct: every non-[A-Za-z0-9._-] becomes '_', no padding.
        const std::string want = "BanG_Dream__It_s_MyGO_____";
        if (sanitizeForPath("BanG Dream! It's MyGO!!!!!") != want) {
            std::fprintf(stderr, "FAIL: sanitizer for English+punct wrong: got '%s' want '%s'\n",
                         sanitizeForPath("BanG Dream! It's MyGO!!!!!").c_str(), want.c_str());
            return 1;
        }
    }
    // 65+ char input gets capped.
    {
        const std::string s = sanitizeForPath(std::string(80, 'a'));
        if (s.size() != 64) {
            std::fprintf(stderr, "FAIL: sanitizer should cap to 64, got %zu\n", s.size());
            return 1;
        }
    }

    // Make a per-test cache dir.
    fs::path tmpRoot = fs::temp_directory_path() / "aniswitch-webcache-test";
    std::error_code ec;
    fs::remove_all(tmpRoot, ec);
    fs::create_directories(tmpRoot, ec);
    if (ec) {
        std::fprintf(stderr, "FAIL: cannot create %s: %s\n",
                     tmpRoot.string().c_str(), ec.message().c_str());
        return 1;
    }

    // Empty cache: readSearchHtml returns "".
    if (int r = expectEq(readSearchHtml(tmpRoot.string(), "酱紫社(修复)", "BanG Dream!"), "",
                         "readSearchHtml on empty cache"); r) return r;

    // Empty dir: also "" (early-return).
    if (int r = expectEq(readSearchHtml("", "x", "y"), "", "readSearchHtml empty dir"); r) return r;

    // Drop a search HTML file with the exact name the provider will probe.
    const std::string sourceName = "酱紫社(修复)";
    const std::string keyword    = "BanG Dream! It's MyGO!!!!!";
    fs::create_directories(tmpRoot / sanitizeForPath(sourceName), ec);
    const std::string searchFile =
        (tmpRoot / sanitizeForPath(sourceName) / ("search_" + sanitizeForPath(keyword) + ".html")).string();
    {
        std::ofstream f(searchFile, std::ios::binary);
        f << "<html>cached-search-html</html>";
    }
    if (int r = expectEq(readSearchHtml(tmpRoot.string(), sourceName, keyword),
                         "<html>cached-search-html</html>", "search cache hit"); r) return r;

    // Different keyword → no hit.
    if (int r = expectEq(readSearchHtml(tmpRoot.string(), sourceName, "Lycoris Recoil"), "",
                         "search cache miss on unrelated keyword"); r) return r;

    // Drop a detail HTML file and verify the new readDetailHtml.
    const std::string detailFile =
        (tmpRoot / sanitizeForPath(sourceName) / ("detail_" + sanitizeForPath(keyword) + ".html")).string();
    {
        std::ofstream f(detailFile, std::ios::binary);
        f << "<html>cached-detail-html</html>";
    }
    if (int r = expectEq(readDetailHtml(tmpRoot.string(), sourceName, keyword),
                         "<html>cached-detail-html</html>", "detail cache hit"); r) return r;
    if (int r = expectEq(readDetailHtml(tmpRoot.string(), sourceName, "Lycoris Recoil"), "",
                         "detail cache miss on unrelated keyword"); r) return r;

    // Empty dir disables both.
    if (int r = expectEq(readDetailHtml("", sourceName, keyword), "",
                         "readDetailHtml empty dir"); r) return r;

    // Clean up.
    fs::remove_all(tmpRoot, ec);

    std::puts("test_web_selector_cache: OK");
    return 0;
}
