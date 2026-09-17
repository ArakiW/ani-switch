// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
#include "utils/dns_helper.hpp"
#include <borealis/core/logger.hpp>
#include <cstring>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

namespace aniswitch {

DNSHelper::DNSHelper() {
    dns4 = "udp://8.8.8.8:53";
    dns6 = "udp://[2001:4860:4860::8888]:53";
}

DNSHelper::~DNSHelper() { stop(); }

void DNSHelper::start() {
    if (running_.exchange(true)) return;
    worker_ = std::thread(&DNSHelper::loop, this);
    brls::Logger::info("DNSHelper started ({} / {})", dns4, dns6);
}

void DNSHelper::stop() {
    if (!running_.exchange(false)) return;
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void DNSHelper::setDNSServer(const std::string& v4, const std::string& v6) {
    if (!v4.empty()) dns4 = v4;
    if (!v6.empty()) dns6 = v6;
}

void DNSHelper::setDNSTimeout(int timeoutMs) { timeout_ = timeoutMs; }
void DNSHelper::setDNSCacheTime(int cacheMs) {
    std::lock_guard<std::mutex> lock(mu_);
    cache_ttl_ms_ = cacheMs;
}

void DNSHelper::resolve(const std::string& host, std::function<void(const std::string&)> cb) {
    if (host.empty()) {
        if (cb) cb("");
        return;
    }
    std::string cached;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto& entry = cache_[host];
        if (entry.available() && entry.expires > std::chrono::steady_clock::now()) {
            cached = entry.ip;
        } else {
            entry.ip.clear();
            entry.callbacks.push_back(cb);
            entry.requesting.store(true);
        }
    }
    if (!cached.empty()) {
        if (cb) cb(cached);
        return;
    }
    cv_.notify_one();
}

void DNSHelper::loop() {
    while (running_.load()) {
        std::string host;
        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait_for(lk, std::chrono::milliseconds(200), [&]{
                if (!running_.load()) return true;
                for (auto& kv : cache_) {
                    if (kv.second.requesting_now() && !kv.second.available()) return true;
                }
                return false;
            });
            if (!running_.load()) break;
            for (auto& kv : cache_) {
                if (kv.second.requesting_now() && !kv.second.available()) {
                    host = kv.first;
                    break;
                }
            }
        }
        if (host.empty()) continue;

        // We don't actually use mongoose's resolver here — system getaddrinfo
        // is fine for our purposes; the DNSHelper exists for the wiliwili
        // contract and for future udp:// override. We just consult the
        // system resolver synchronously on this thread.
        struct addrinfo hints {};
        hints.ai_family = AF_INET;
        struct addrinfo* res = nullptr;
        std::string ip;
        if (getaddrinfo(host.c_str(), nullptr, &hints, &res) == 0 && res) {
            char buf[64] = {0};
            const auto* sa = reinterpret_cast<sockaddr_in*>(res->ai_addr);
            inet_ntop(AF_INET, &sa->sin_addr, buf, sizeof(buf));
            ip = buf;
            freeaddrinfo(res);
        }

        std::vector<std::function<void(const std::string&)>> cbs;
        {
            std::lock_guard<std::mutex> lk(mu_);
            auto it = cache_.find(host);
            if (it != cache_.end()) {
                it->second.ip = ip;
                it->second.expires = std::chrono::steady_clock::now() + std::chrono::milliseconds(cache_ttl_ms_);
                it->second.requesting.store(false);
                cbs = std::move(it->second.callbacks);
                it->second.callbacks.clear();
            }
        }
        for (auto& cb : cbs) {
            try { cb(ip); } catch (...) {}
        }
    }
}

}  // namespace aniswitch
