// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
//
// Async DNS resolver over UDP, using mongoose for socket IO.
// Used by the HTTP layer when the Switch system resolver is slow or
// unreliable.

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <chrono>


namespace aniswitch {

class DNSResult {
public:
    std::string host;
    std::vector<std::function<void(const std::string&)>> callbacks;
    std::string ip;
    std::chrono::steady_clock::time_point expires{};
    std::atomic<bool> requesting{false};

    bool available() const { return !ip.empty(); }
    bool requesting_now() const { return requesting.load(); }
};

class DNSHelper {
public:
    DNSHelper();
    ~DNSHelper();

    void start();
    void stop();

    // Resolve host -> ipv4 string. Callback is invoked on the resolver thread.
    void resolve(const std::string& host, std::function<void(const std::string&)> cb);

    void setDNSServer(const std::string& v4, const std::string& v6 = "");
    void setDNSTimeout(int timeoutMs = 3000);
    void setDNSCacheTime(int cacheMs = 60000);

private:
    void loop();

    std::string dns4;
    std::string dns6;
    int timeout_ = 3000;
    int cache_ttl_ms_ = 60000;

    std::thread worker_;
    std::atomic<bool> running_{false};
    std::mutex mu_;
    std::condition_variable cv_;
    std::unordered_map<std::string, DNSResult> cache_;
};

}  // namespace aniswitch
