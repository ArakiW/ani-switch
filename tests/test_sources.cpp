// SPDX-License-Identifier: AGPL-3.0
#include "core/source_manager.hpp"
#include <cstdio>
#include <cstdlib>

#define EXPECT(c) do { if (!(c)) { std::fprintf(stderr,"FAIL: %s:%d\n",#c,__LINE__); std::exit(1); } } while(0)

class Source : public aniswitch::ISourceProvider {
public:
    aniswitch::SourceKind kind() const override { return aniswitch::SourceKind::HTTP; }
    void enumerate(int32_t, aniswitch::SourceListCb cb, std::function<void(const std::string&, int)> err) override {
        result = std::move(cb); error = std::move(err);
    }
    aniswitch::SourceListCb result;
    std::function<void(const std::string&, int)> error;
};

int main() {
    using namespace aniswitch;
    auto& manager = SourceManager::instance();
    int calls = 0, errors = 0;
    manager.enumerate(1, [&](std::vector<VideoSource> list) { EXPECT(list.empty()); ++calls; }, {});
    EXPECT(calls == 1);
    auto first = std::make_shared<Source>(), second = std::make_shared<Source>();
    manager.registerProvider(first); manager.registerProvider(second);
    manager.enumerate(1, [&](std::vector<VideoSource> list) {
        ++calls;
        EXPECT(list.size() == 1 && list[0].url == "http://localhost/video.mp4");
    }, [&](const std::string&, int) { ++errors; });
    VideoSource video; video.url = "http://localhost/video.mp4";
    second->result({video});
    EXPECT(calls == 1);
    first->error("unavailable", 503);
    EXPECT(calls == 2 && errors == 0);
    manager.enumerate(1, [&](std::vector<VideoSource>) { ++calls; }, [&](const std::string&, int) { ++errors; });
    first->error("first", 500); second->error("second", 500);
    EXPECT(errors == 1 && calls == 2);
    VideoSource high = video; high.bitrate = 2000;
    EXPECT(manager.pickBest({video, high})->bitrate == 2000);
    EXPECT(!manager.pickBest({}));
    std::puts("test_sources: OK");
}
