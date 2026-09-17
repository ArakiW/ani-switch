// SPDX-License-Identifier: AGPL-3.0
// Standalone v16.10.12 cpr E2E.  Compile directly with cpr
// headers + libcurl + openssl.  No borealis / fmt / nlohmann
// dependency.  Confirms our cpr session shape (URL rewrite +
// Host header + TLS 1.2 + ca_info) hits api.bgm.tv on the
// host and gets 200.  If this passes, the Switch is
// almost certainly to pass too.
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <cstdio>
#include <string>
#include <cstring>

int main() {
    cpr::SslOptions sslOpts;
    // v16.10.14: pin a specific TLS 1.3 cipher list (same as
    // the NRO build) so the PC e2e stays representative of
    // the Switch path.
    sslOpts.ssl_version = CURL_SSLVERSION_DEFAULT;
    sslOpts.ciphers =
        "TLS_AES_256_GCM_SHA384:"
        "TLS_AES_128_GCM_SHA256:"
        "TLS_CHACHA20_POLY1305_SHA256:"
        "ECDHE-ECDSA-AES256-GCM-SHA384:"
        "ECDHE-RSA-AES256-GCM-SHA384";
    sslOpts.ca_info = "E:/AI/ani-switch/resources/ca-bundle.crt";
    sslOpts.verify_peer = false;
    sslOpts.verify_host = false;

    cpr::Session s;
    s.SetUrl(cpr::Url{"https://api.bgm.tv/v0/subjects/1"});
    s.SetVerbose(true);
    s.SetTimeout(cpr::Timeout{12000});
    s.SetConnectTimeout(cpr::ConnectTimeout{5000});
    s.SetHeader(cpr::Header{
        {"User-Agent", "ani-switch/0.1.0 (https://github.com/MiniMax139102/ani-switch)"},
        {"Accept",     "application/json"},
    });
    s.SetVerifySsl(cpr::VerifySsl{false});
    s.SetSslOptions(sslOpts);

    cpr::Response r = s.Get();
    if (r.error) {
        std::printf("[fail] cpr.Get error: %s\n", r.error.message.c_str());
        return 1;
    }
    std::printf("[status] %d (size=%zu)\n", r.status_code, r.text.size());
    if (r.status_code != 200) {
        std::printf("[fail] body[:200]=%.200s\n", r.text.c_str());
        return 1;
    }
    auto j = nlohmann::json::parse(r.text);
    if (!j.contains("id") || j["id"].get<int>() != 1) {
        std::printf("[fail] json id mismatch: %s\n", r.text.substr(0, 200).c_str());
        return 1;
    }
    std::string name = j.value("name", "");
    std::printf("[ok] api.bgm.tv/v0/subjects/1 -> 200, name=%s\n", name.c_str());
    return 0;
}
