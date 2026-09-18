// SPDX-License-Identifier: AGPL-3.0
//
// HTTPSourceProvider sources.json lookup — no network, no cpr.
// Covers: episode-id key hit, subject-id fallback, first-key-wins,
// missing file soft-empty, BOM, corrupt JSON soft-empty, non-http skip.
#include "core/http_source.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#define EXPECT(c)                                                              \
    do {                                                                       \
        if (!(c)) {                                                            \
            std::fprintf(stderr, "FAIL: %s:%d %s\n", __FILE__, __LINE__, #c); \
            std::exit(1);                                                      \
        }                                                                      \
    } while (0)

namespace {

std::string writeTemp(const std::string& name, const std::string& body) {
    const std::string path = std::string("http_source_test_") + name;
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f << body;
    f.close();
    return path;
}

}  // namespace

int main() {
    using namespace aniswitch;

    // Missing file → soft empty, no throw.
    HTTPSourceProvider::setManifestPathOverride("definitely_missing_sources.json");
    HTTPSourceProvider::resetManifestCache();
    auto miss = HTTPSourceProvider::lookup({1227087});
    EXPECT(miss.empty());
    EXPECT(!HTTPSourceProvider::manifestOk());

    // Documented working shape: episode-id keys (Bangumi 集数 ID).
    const char* kJson =
        "{\"1227087\":[{\"url\":\"https://c1.rrcdnbf5.com/video/frieren/ep1/index.m3u8\","
        "\"label\":\"秋之动漫 · 葬送的芙莉莲 EP1\",\"priority\":10}],"
        "\"400602\":[{\"url\":\"https://c1.rrcdnbf5.com/video/frieren/ep1/index.m3u8\","
        "\"label\":\"subject-alias\",\"priority\":5}],"
        "\"999001\":[{\"url\":\"not-a-http-url\",\"label\":\"bad\",\"priority\":1}],"
        "\"999002\":[{\"url\":\"http://ok.example/v.mp4\",\"label\":\"ok\",\"priority\":1},"
        "{\"url\":\"ftp://skip.me\",\"label\":\"skip\"}]}";
    const std::string path = writeTemp("ok.json", kJson);
    HTTPSourceProvider::setManifestPathOverride(path);
    HTTPSourceProvider::resetManifestCache();
    EXPECT(HTTPSourceProvider::manifestOk());
    EXPECT(HTTPSourceProvider::manifestKeyCount() == 4);

    // Primary: bangumi episode id.
    auto hit = HTTPSourceProvider::lookup({1227087});
    EXPECT(hit.size() == 1);
    EXPECT(hit[0].url.find("index.m3u8") != std::string::npos);
    EXPECT(hit[0].label.find("芙莉莲") != std::string::npos);
    EXPECT(hit[0].kind == SourceKind::HTTP);
    EXPECT(hit[0].priority == 10);

    // Subject-id fallback when episode key is absent.
    auto bySubject = HTTPSourceProvider::lookup({1656858, 400602});
    EXPECT(bySubject.size() == 1);
    EXPECT(bySubject[0].label == "subject-alias");

    // First key wins — episode key preferred over subject alias.
    auto both = HTTPSourceProvider::lookup({1227087, 400602});
    EXPECT(both.size() == 1);
    EXPECT(both[0].label.find("芙莉莲") != std::string::npos);

    // Unknown key → empty, not error.
    auto none = HTTPSourceProvider::lookup({31337});
    EXPECT(none.empty());

    // Non-http URL alone → empty (entry skipped, provider soft-succeeds).
    auto badOnly = HTTPSourceProvider::lookup({999001});
    EXPECT(badOnly.empty());

    // Mixed array: good entry kept, bad skipped.
    auto mixed = HTTPSourceProvider::lookup({999002});
    EXPECT(mixed.size() == 1);
    EXPECT(mixed[0].url.rfind("http://", 0) == 0);

    // ISourceProvider::enumerate soft-succeeds empty on miss (no error cb).
    HTTPSourceProvider provider;
    bool cbCalled = false, errCalled = false;
    provider.enumerate(31337,
                       [&](std::vector<VideoSource> v) {
                           cbCalled = true;
                           EXPECT(v.empty());
                       },
                       [&](const std::string&, int) { errCalled = true; });
    EXPECT(cbCalled);
    EXPECT(!errCalled);

    // enumerate hit path.
    cbCalled = false;
    provider.enumerate(1227087,
                       [&](std::vector<VideoSource> v) {
                           cbCalled = true;
                           EXPECT(v.size() == 1);
                       },
                       [&](const std::string&, int) { errCalled = true; });
    EXPECT(cbCalled);
    EXPECT(!errCalled);

    // UTF-8 BOM must not break parse.
    const std::string bomPath =
        writeTemp("bom.json", std::string("\xEF\xBB\xBF") + kJson);
    HTTPSourceProvider::setManifestPathOverride(bomPath);
    HTTPSourceProvider::resetManifestCache();
    EXPECT(HTTPSourceProvider::manifestOk());
    auto bomHit = HTTPSourceProvider::lookup({1227087});
    EXPECT(bomHit.size() == 1);

    // Corrupt JSON → soft empty, not a hard error.
    const std::string badPath = writeTemp("bad.json", "{not json!!");
    HTTPSourceProvider::setManifestPathOverride(badPath);
    HTTPSourceProvider::resetManifestCache();
    EXPECT(!HTTPSourceProvider::manifestOk());
    auto corrupt = HTTPSourceProvider::lookup({1227087});
    EXPECT(corrupt.empty());
    cbCalled = errCalled = false;
    provider.enumerate(1227087,
                       [&](std::vector<VideoSource> v) {
                           cbCalled = true;
                           EXPECT(v.empty());
                       },
                       [&](const std::string&, int) { errCalled = true; });
    EXPECT(cbCalled);
    EXPECT(!errCalled);

    HTTPSourceProvider::setManifestPathOverride("");
    HTTPSourceProvider::resetManifestCache();
    std::puts("test_http_sources: OK");
    return 0;
}
