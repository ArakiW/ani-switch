// SPDX-License-Identifier: AGPL-3.0
#include "ui/presenter/presenter.hpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace {
std::mutex mutex;
std::vector<std::function<void()>> pending;
void drain() {
    std::vector<std::function<void()>> work;
    { std::lock_guard<std::mutex> lock(mutex); work.swap(pending); }
    for (auto& task : work) task();
}
}

namespace brls {
void sync(const std::function<void()>& callback) {
    std::lock_guard<std::mutex> lock(mutex);
    pending.push_back(callback);
}
}

class TestPresenter : public aniswitch::Presenter {
public:
    auto callback(int& result) {
        return uiCallback([&result](int value) { result += value; });
    }
};

#define EXPECT(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "FAIL: %s at %d\n", #condition, __LINE__); return 1; } } while (0)

int main() {
    int result = 0;
    std::function<void(int)> late;
    {
        TestPresenter presenter;
        late = presenter.callback(result);
        std::thread network([&] { late(4); });
        network.join();
        EXPECT(result == 0);
        drain();
        EXPECT(result == 4);
        late(8);
    }
    drain();
    EXPECT(result == 4);
    late(16);
    drain();
    EXPECT(result == 4);
    std::puts("test_presenter_lifetime: OK");
}
