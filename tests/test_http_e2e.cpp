// SPDX-License-Identifier: AGPL-3.0
// E2E test for the v16.10.12 HTTP path: cpr session with the same
// SslOptions + URL-rewrite + SetHeader/UpdateHeader choices we ship
// to the Switch.  Hits https://api.bgm.tv/v0/subjects/1 over the
// live internet and asserts status==200.  Skipped (returns 0) if
// the network is unavailable so the test stays hostable on an
// offline developer machine.

#include "net/http.hpp"
#include "net/bgm_client.hpp"
#include "net/bgm_auth.hpp"
#include <cpr/cpr.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <curl/curl.h>

namespace {

int dnsCurlResolve() {
    // cpr ships its own DNS path on top of c-ares / getaddrinfo; we
    // just check that the host resolves to anything at all so the
    // E2E test is meaningful.  Print the first A record we find.
    const char* host = "api.bgm.tv";
    struct hostent* he = gethostbyname(host);
    if (!he || he->h_addr_list[0] == nullptr) {
        std::printf("[skip] DNS lookup for %s failed\n", host);
        return -1;
    }
    char ip[64] = {0};
    unsigned char* a = reinterpret_cast<unsigned char*>(he->h_addr_list[0]);
    std::snprintf(ip, sizeof(ip), "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
    std::printf("[ok] DNS lookup for %s -> %s\n", host, ip);
    return 0;
}

int hitCalendar() {
    // v16.10.12 SslOptions: TLS 1.2, ca_info = bundle, verify off.
    // Same code path the NRO uses; if PC works, Switch is very
    // likely to work too (cpr session shape is identical).
    cpr::SslOptions sslOpts;
    sslOpts.ssl_version = CURL_SSLVERSION_TLSv1_2;
    sslOpts.ca_info = HTTP::SSL_CA_PATH;
    sslOpts.verify_peer = false;
    sslOpts.verify_host = false;

    cpr::Session s;
    s.SetUrl(cpr::Url{"https://api.bgm.tv/v0/subjects/1"});
    s.SetTimeout(cpr::Timeout{HTTP::TIMEOUT});
    s.SetConnectTimeout(cpr::ConnectTimeout{HTTP::CONNECTION_TIMEOUT});
    s.SetHeader(HTTP::HEADERS);
    s.SetVerifySsl(HTTP::VERIFY);
    s.SetSslOptions(sslOpts);

    // Simulate v16.10.8 URL rewrite: rewrite api.bgm.tv -> 104.26.8.23
    // and add a Host header.  Same helper the NRO uses, but
    // bypass the Switch-only anonymous-namespace lookup.
    std::string effectiveUrl = "https://104.26.8.23/v0/subjects/1";
    s.SetUrl(cpr::Url{effectiveUrl});
    s.UpdateHeader(cpr::Header{{"Host", "api.bgm.tv"}});

    cpr::Response r = s.Get();
    if (r.error) {
        std::printf("[fail] cpr.Get returned error: %s\n",
                    r.error.message.c_str());
        return 1;
    }
    if (r.status_code != 200) {
        std::printf("[fail] status=%d body=%s\n", r.status_code,
                    r.text.substr(0, 200).c_str());
        return 1;
    }
    // Light sanity check: the v0/subjects/1 response is a JSON
    // object with at least an `id` and `name` field.  Just
    // confirm it parses and the id matches.
    auto j = nlohmann::json::parse(r.text);
    if (!j.contains("id") || j["id"].get<int>() != 1) {
        std::printf("[fail] JSON missing expected id=1: %s\n",
                    r.text.substr(0, 200).c_str());
        return 1;
    }
    std::string name = j.value("name", "");
    std::printf("[ok] api.bgm.tv/v0/subjects/1 -> 200, name=\"%s\"\n",
                name.c_str());
    return 0;
}

}  // namespace

int main() {
    if (dnsCurlResolve() != 0) {
        std::printf("[skip] no network, exiting 0\n");
        return 0;
    }
    return hitCalendar();
}
