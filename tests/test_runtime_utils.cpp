// SPDX-License-Identifier: AGPL-3.0
#ifdef _WIN32
#include <winsock2.h>
#endif
#include "utils/thread_helper.hpp"
#include "utils/dns_helper.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <future>
#include <stdexcept>

#define EXPECT(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "FAIL: %s at %d\n", #condition, __LINE__); std::exit(1); } } while (0)

int main() {
    using namespace aniswitch;
    using namespace std::chrono_literals;
    std::atomic<int> count{0};
    {
        ThreadPool first(1), second(2);
        for (int i = 0; i < 100; ++i) first.submit([&] { ++count; });
        first.shutdown();
        EXPECT(count == 100);
        second.submit([&] { ++count; });
        second.shutdown();
        EXPECT(count == 101);
        first.shutdown();
        bool rejected = false;
        try { first.submit([] {}); } catch (const std::runtime_error&) { rejected = true; }
        EXPECT(rejected);
    }
    {
        ThreadPool recreated(1);
        recreated.submit([&] { ++count; });
    }
    EXPECT(count == 102);

#ifdef _WIN32
    WSADATA data{};
    EXPECT(WSAStartup(MAKEWORD(2, 2), &data) == 0);
#endif
    {
        DNSHelper dns;
        dns.setDNSCacheTime(60000);
        dns.start();
        std::promise<std::string> first;
        auto result = first.get_future();
        dns.resolve("localhost", [&](const std::string& ip) { first.set_value(ip); });
        EXPECT(result.wait_for(3s) == std::future_status::ready);
        EXPECT(!result.get().empty());
        std::promise<std::string> nested;
        auto next = nested.get_future();
        dns.resolve("localhost", [&](const std::string&) {
            dns.resolve("localhost", [&](const std::string& ip) { nested.set_value(ip); });
        });
        EXPECT(next.wait_for(3s) == std::future_status::ready);
        EXPECT(!next.get().empty());
        dns.stop();
    }
    {
        DNSHelper dns;
        dns.setDNSCacheTime(0);
        dns.start();
        for (int i = 0; i < 2; ++i) {
            std::promise<std::string> reply;
            auto result = reply.get_future();
            dns.resolve("localhost", [&](const std::string& ip) { reply.set_value(ip); });
            EXPECT(result.wait_for(3s) == std::future_status::ready);
            EXPECT(!result.get().empty());
        }
        dns.stop();
    }
#ifdef _WIN32
    WSACleanup();
#endif
    std::puts("test_runtime_utils: OK");
}
