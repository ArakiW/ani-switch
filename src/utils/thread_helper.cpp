// SPDX-License-Identifier: AGPL-3.0
#include "utils/thread_helper.hpp"
#include <stdexcept>
#include <borealis/core/logger.hpp>

namespace aniswitch {

void ThreadPool::loop() {
    for (;;) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            ready_.wait(lock, [this] { return !running_ || !queue_.empty(); });
            if (queue_.empty()) return;
            job = std::move(queue_.front());
            queue_.pop();
        }
        try {
            job();
        } catch (const std::exception& e) {
            brls::Logger::error("ThreadPool task threw: {}", e.what());
        } catch (...) {
            brls::Logger::error("ThreadPool task threw an unknown exception");
        }
    }
}

ThreadPool::ThreadPool(size_t workers) {
    if (workers == 0) throw std::invalid_argument("ThreadPool requires a worker");
    workers_.reserve(workers);
    try {
        for (size_t i = 0; i < workers; ++i)
            workers_.emplace_back([this] { loop(); });
    } catch (...) {
        shutdown();
        throw;
    }
}

ThreadPool::~ThreadPool() { shutdown(); }

void ThreadPool::submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) throw std::runtime_error("ThreadPool has stopped");
        queue_.push(std::move(task));
    }
    ready_.notify_one();
}

void ThreadPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
    }
    ready_.notify_all();
    for (auto& worker : workers_)
        if (worker.joinable()) worker.join();
    workers_.clear();
}

void submit_detached(std::function<void()> task) {
    static ThreadPool pool(4);
    pool.submit(std::move(task));
}

}  // namespace aniswitch
