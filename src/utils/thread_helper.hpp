// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
#pragma once

#include <functional>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace aniswitch {

// Fixed-size worker pool for fire-and-forget background tasks (image
// decoding, network IO, etc). Identical contract to wiliwili's thread
// helper.
class ThreadPool {
public:
    explicit ThreadPool(size_t workers = 4);
    ~ThreadPool();

    void submit(std::function<void()> task);
    void shutdown();

private:
    std::mutex mutex_;
    std::condition_variable ready_;
    std::queue<std::function<void()>> queue_;
    bool running_ = true;
    std::vector<std::thread> workers_;
    void loop();
};

// Submit a one-off detached task (no shared lifetime).
void submit_detached(std::function<void()> task);

}  // namespace aniswitch
