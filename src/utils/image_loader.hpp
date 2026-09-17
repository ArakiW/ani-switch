// SPDX-License-Identifier: AGPL-3.0
//
// v21.0: serial image download queue.  The old design spawned a
// detached std::thread per URL which crashed the Switch NRO as
// soon as home kicked off cover loads.  One long-lived worker
// drains a FIFO; UI callbacks are marshalled with brls::sync.
#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace aniswitch {

class ImageLoader {
public:
    using LoadCb = std::function<void(const std::string& localPathOrEmpty)>;

    static ImageLoader& instance();

    void setCacheDir(const std::string& dir);

    // Queue `url` for download.  Invokes `cb` with a local file
    // path (or "" on failure).  `cb` may run on the worker thread.
    void load(const std::string& url,
              LoadCb cb,
              const std::string& tag = "");

    void clearCache();
    std::string cachePathFor(const std::string& url) const;

private:
    ImageLoader();
    ~ImageLoader();
    ImageLoader(const ImageLoader&) = delete;
    ImageLoader& operator=(const ImageLoader&) = delete;

    struct Job {
        std::string url;
        std::string path;
        std::string key;
        LoadCb cb;
    };

    void ensureWorker();
    void loop();
    void downloadOne(const Job& job);

    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::string cacheDir_;
    std::deque<Job> queue_;
    std::unordered_map<std::string, bool> queued_;  // key → in queue
    std::thread worker_;
    bool running_ = false;
    bool stop_ = false;
};

}  // namespace aniswitch
