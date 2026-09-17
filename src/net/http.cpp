// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)

#include "net/http.hpp"
#include <borealis/core/logger.hpp>
#if defined(__SWITCH__)
#include <unordered_map>  // v16.10.9: DNS cache
#endif
#include <nlohmann/json.hpp>

#if defined(__SWITCH__)
// `brls::Logger::info` on the Switch only writes to nxlink / hbmenu
// console (the user has `nxlink unavailable` in startup.log, so it
// goes nowhere).  We need HTTP traffic to land in
// `sdmc:/switch/aniswitch/startup.log` so the next crash tells us
// which URL is hanging.  Declare the C entry point defined in
// `platform/switch/switch_wrapper.c` and call it directly.
extern "C" void aniswitchStartupLog(const char* message);
extern "C" int  aniswitchResolveHost(const char* host, char* out_ip, size_t out_len);
#else
static inline void aniswitchStartupLog(const char* message) {
    if (message) brls::Logger::info("{}", message);
}
#endif

#if defined(__SWITCH__)
// v19.0.2 case A: drop the v16.10.8 hardcoded IP table.  The
// Cloudflare anycast edges that api.bgm.tv / bgm.tv / lain.bgm.tv
// resolve to rotate regularly (typically daily), and a stale IP
// in the table produced `Connection reset by peer` once the
// user saw the old address handed back by the kernel's SYN —
// the IP was no longer accepting new connections.  The raw UDP
// DNS resolver in `switch_wrapper.c` queries 8.8.8.8:53 with
// a 5 s timeout and caches results for the process lifetime,
// which is enough to pick up a fresh IP on the next launch
// without bothering the user.  If 8.8.8.8 is itself
// unreachable (captive portal, ISP firewall) we return -1
// here and the HTTP layer reports a clean error rather than
// the misleading TCP RST we used to see.
#endif

namespace aniswitch {

namespace {

#if defined(__SWITCH__)
// brls::Logger::info doesn't reach the startup log file on Switch
// (only the brls console buffer, which is then thrown away).  A
// one-line printf through aniswitchStartupLog does.  We pay a
// 256-byte stack allocation per HTTP call, which is fine.
void httpLog(const char* fmt, const std::string& url, int status, const char* err) {
    char buf[256];
    if (err && *err) {
        snprintf(buf, sizeof(buf), fmt, url.c_str(), status, err);
    } else {
        snprintf(buf, sizeof(buf), fmt, url.c_str(), status, "(none)");
    }
    aniswitchStartupLog(buf);
}
#endif

#if defined(__SWITCH__)
// v19.0.4 case B' activation: bypass cpr/curl/mbedTLS path and
// talk directly to mbedTLS 2.28 via HTTP::directFetch.  The
// v19.0.2 case B' skeleton landed `mbedtls_direct` + `directFetch`
// as dead code (USE_DIRECT_TLS=false, no caller); the v19.0.4
// patch wires the existing _cpr_get / _cpr_post call sites
// through to the new path so a settings-UI flip can route
// production traffic to mbedTLS direct when cpr/curl can't
// reach Cloudflare.
//
// URL-encoding: cpr encodes Parameters / Payload at
// GetContent() time (not at construction — see
// third_party/cpr/cpr/curl_container.cpp:21-53), so we share
// one CurlHolder for the query string + the POST body to match
// cpr's exact wire form (`key=value&key=value` with
// curl_easy_escape on keys+values for Parameters, on values
// only for the Pair-based Payload).
//
// The result is mapped back to a cpr::Response so the rest of
// the call chain (HTTP::parseJson<T>, postVoid, the user
// callbacks in bgm_client.cpp / dandanplay_client.cpp /
// episode_resolver.cpp) does not need to know that the wire
// went through mbedTLS direct.  cpr::Error is set to
// ErrorCode::SSL_CONNECT_ERROR on transport failure so callers
// like `r.error ? ...` keep working.
//
// Status handling: the existing _cpr_post / _cpr_get accept
// 200 or 204 only.  The directive sketch uses 200-299 inclusive
// (more permissive — 201/202/etc. go through as success); we
// follow the directive because the user is explicit and the
// wider range matches what a real API client expects.
//
// Scope: only the two cpr entry points above the line
// (`_cpr_get` and `_cpr_post`) are routed here.  The async
// GetCallback/PostCallback paths in `bgm_client.cpp`,
// `dandanplay_client.cpp`, `bgm_auth.cpp`, and `ani_client.cpp`
// are out of scope (they never go through `_cpr_*`).
void directCallBypass(const std::string& url,
                      const cpr::Parameters& parameters,
                      const cpr::Payload&    payload,
                      const std::string&     method,   // "GET" or "POST"
                      std::function<void(const cpr::Response&)> cb,
                      ErrorCallback          err,
                      long                   timeout_ms) {
    cpr::CurlHolder holder;  // cpr::util::urlEncode() creates one
                             // internally per call; we share so
                             // curl_easy_init runs once.
    std::string qs   = parameters.GetContent(holder);
    std::string body = (method == "POST") ? payload.GetContent(holder) : std::string();
    std::string fullUrl = url;
    if (!qs.empty()) {
        fullUrl += (url.find('?') == std::string::npos) ? "?" : "&";
        fullUrl += qs;
    }
    std::vector<std::string> hdrs;
    if (method == "POST") {
        // cpr::Session::SetPayload defaults to this Content-Type;
        // match it so the server parses the body the same way.
        hdrs.push_back("Content-Type: application/x-www-form-urlencoded");
    }
    auto uaIt = HTTP::HEADERS.find("User-Agent");
    hdrs.push_back("User-Agent: " +
                   (uaIt != HTTP::HEADERS.end() ? uaIt->second
                                                : std::string("ani-switch/0.1.0")));

    char _b[220];
    std::snprintf(_b, sizeof(_b),
                  "HTTP: %s %s (direct TLS bypass, qs=%zu B, body=%zu B)",
                  method.c_str(), url.c_str(), qs.size(), body.size());
    aniswitchStartupLog(_b);

    auto r = HTTP::directFetch(fullUrl, method, body, hdrs, timeout_ms);

    cpr::Response fakeR;        // default-constructed
    fakeR.status_code = r.status;
    fakeR.text        = r.body;
    fakeR.elapsed     = r.elapsed_ms;
    if (!r.error.empty()) {
        // Transport / TLS / parse error from mbedtls_direct.
        // cpr::Error::operator bool() is true iff code != OK,
        // so the existing `if (r.error)` checks downstream keep
        // working.
        fakeR.error.code    = cpr::ErrorCode::SSL_CONNECT_ERROR;
        fakeR.error.message = r.error;
        char _e[220];
        std::snprintf(_e, sizeof(_e),
                      "HTTP: %s %s -> direct TLS error: %s",
                      method.c_str(), url.c_str(), r.error.c_str());
        aniswitchStartupLog(_e);
        fireError(err, r.error, r.status);
        return;
    }
    char _s[160];
    std::snprintf(_s, sizeof(_s),
                  "HTTP: %s %s -> direct TLS status=%ld body=%zu B",
                  method.c_str(), url.c_str(), r.status, r.body.size());
    aniswitchStartupLog(_s);
    // Per directive: 200-299 inclusive is "success".  More
    // permissive than the existing cpr path (which only accepts
    // 200/204); lets 201/202/203 through as success.
    if (r.status >= 200 && r.status < 300) {
        cb(fakeR);
    } else {
        std::string msg = std::string("Network error. [") + std::to_string(r.status) + "]";
        fireError(err, msg, r.status);
    }
}
#endif

}  // namespace

void HTTP::applyProxy(const std::string& url) {
    PROXY_URL.clear();
    PROXIES = cpr::Proxies{};
    std::string u = url;
    while (!u.empty() && (u.back() == ' ' || u.back() == '\n' || u.back() == '\r')) u.pop_back();
    while (!u.empty() && u.front() == ' ') u.erase(u.begin());
    if (u.empty()) {
        aniswitchStartupLog("HTTP: proxy cleared");
        return;
    }
    if (u.find("://") == std::string::npos) {
        u = "http://" + u;
    }
    PROXY_URL = u;
    // curl understands http:// and socks5:// (and socks5h://) via CURLOPT_PROXY.
    PROXIES = cpr::Proxies{{"http", u}, {"https", u}};
    char buf[256];
    snprintf(buf, sizeof(buf), "HTTP: proxy set %s", u.c_str());
    aniswitchStartupLog(buf);
}

std::string HTTP::proxyHint() {
    if (PROXY_URL.empty()) {
        return "未配置代理。若直连失败，请在电脑 Clash 打开 Allow LAN，"
               "然后在设置里填写 http://<电脑IP>:7897";
    }
    return "代理: " + PROXY_URL;
}

std::shared_ptr<cpr::Session> HTTP::createSession() {
    auto session = std::make_shared<cpr::Session>();
    aniswitchStartupLog("ANISWITCH v20.0 proxy-first build marker");
    session->SetTimeout(cpr::Timeout{HTTP::TIMEOUT});
    session->SetConnectTimeout(cpr::ConnectTimeout{HTTP::CONNECTION_TIMEOUT});
    session->SetHeader(HTTP::HEADERS);
    // PROXIES is empty unless applyProxy() was called with a non-empty URL.
    session->SetProxies(HTTP::PROXIES);
    session->SetVerifySsl(HTTP::VERIFY);
    session->SetCookies(HTTP::COOKIES);
    // v18.9.1: TLS / HTTP wire capture.  Curl 8.4's
    // mbedTLS backend does *not* implement
    // `CURLOPT_SSL_CTX_FUNCTION`, hard-codes
    // `MBEDTLS_SSL_VERIFY_OPTIONAL`, and ignores our
    // `CURLOPT_SSL_CIPHER_LIST` (it always uses
    // `mbedtls_ssl_list_ciphersuites()`).  Before we can
    // pick a fix we need to see what is actually on the
    // wire — the client hello cipher list, the
    // `signature_algorithms` extension, whether the server
    // even replies with a ServerHello before the RST, etc.
    // `CURLOPT_DEBUGFUNCTION` is the lowest-cost way to
    // get that.  We do a hex dump of every TLS record
    // (cpr 1.10.5's util.cpp:147 forwards the raw bytes
    // straight into the callback) plus every curl
    // `infof()` text (mbedTLS handshake state lines
    // "mbedTLS: Connecting to ...", "Handshake complete,
    // cipher is ...", "schannel/openssl/mbedTLS: failed
    // to ...", etc.).  Output goes through
    // `aniswitchStartupLog` so it lands in
    // `sdmc:/switch/aniswitch/startup.log` next to the
    // v18.8 LogActivity reader.  One user run should be
    // enough to settle whether the RST happens *before*
    // ServerHello (Cloudflare fingerprint rejection) or
    // *after* the handshake (HTTP/2 / keepalive / SNI
    // routing) — the two fixes are very different.
    if (HTTP::NET_DEBUG_VERBOSE) {
        session->SetVerbose(cpr::Verbose{true});
        session->SetDebugCallback(cpr::DebugCallback{
            // cpr's DebugCallback functor expects a 3-arg signature
            // (InfoType, std::string, intptr_t) — the intptr_t is a
            // userdata cookie forwarded from cpr::util::debugUserFunction.
            [](cpr::DebugCallback::InfoType type, std::string data, intptr_t /*userdata*/) {
#if defined(__SWITCH__)
                static const char* kTypeName[7] = {
                    "TEXT", "HDRIN", "HDROUT", "DATAIN", "DATAOUT",
                    "SSLIN", "SSLOUT",
                };
                int t = static_cast<int>(type);
                const char* tname = (t >= 0 && t < 7) ? kTypeName[t] : "?";
                if (type == cpr::DebugCallback::InfoType::SSL_DATA_IN ||
                    type == cpr::DebugCallback::InfoType::SSL_DATA_OUT) {
                    // Hex dump — TLS records are binary and would
                    // corrupt startup.log if dumped raw.
                    size_t n = data.size();
                    if (n == 0) {
                        char buf[64];
                        snprintf(buf, sizeof(buf),
                                 "CURL: SSL_%s len=0",
                                 tname);
                        aniswitchStartupLog(buf);
                        return;
                    }
                    // First line: header.  Subsequent lines: up
                    // to 32 bytes of hex.
                    char hdr[64];
                    snprintf(hdr, sizeof(hdr),
                             "CURL: SSL_%s len=%zu (first 96 B):",
                             tname, n);
                    aniswitchStartupLog(hdr);
                    size_t show = n > 96 ? 96 : n;
                    char hex[320];
                    size_t pos = 0;
                    for (size_t i = 0; i < show; i += 32) {
                        size_t end = (i + 32 < show) ? i + 32 : show;
                        size_t off = 0;
                        for (size_t j = i; j < end; ++j) {
                            off += (size_t)snprintf(
                                hex + off, sizeof(hex) - off,
                                "%02x ",
                                static_cast<unsigned char>(data[j]));
                        }
                        hex[off - 1] = 0;  // trim trailing space
                        aniswitchStartupLog(hex);
                        (void)pos;
                    }
                } else {
                    // TEXT / HDRIN / HDROUT / DATAIN / DATAOUT
                    // — strip trailing newline for nicer log
                    // alignment.
                    std::string s = data;
                    while (!s.empty() &&
                           (s.back() == '\n' || s.back() == '\r')) {
                        s.pop_back();
                    }
                    // Cap at 240 chars to keep startup.log readable
                    // if curl dumps a huge header.
                    if (s.size() > 240) s.resize(240);
                    char buf[320];
                    snprintf(buf, sizeof(buf), "CURL: %s %s", tname, s.c_str());
                    aniswitchStartupLog(buf);
                }
#else
                (void)type;
                (void)data;
#endif
            }});
    }
    // v18.0.2: pin to TLS 1.2 only.  Cloudflare's TLS 1.3
    // edge now advertises X25519MLKEM768 (post-quantum KEM
    // group) as its preferred key-share, and mbedTLS 2.28.10
    // (the version on devkitpro 20260219's portlibs) does
    // not carry that KEM.  The server then sends a fatal
    // alert `handshake_failure` / `insufficient_security` and
    // the handshake drops with mbedTLS error -0x7780
    // (MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE).  The v16.10.14
    // cipher list still works in TLS 1.2 mode because every
    // modern Cloudflare POP keeps an ECDHE-RSA / AES-GCM
    // fallback that mbedTLS 2.28.10 can speak.  We lose
    // forward-secrecy-with-quantum-resistance but the
    // alternative is no network at all on the Switch.
    cpr::SslOptions sslOpts;
    sslOpts.ssl_version = CURL_SSLVERSION_TLSv1_2;
    sslOpts.ciphers =
        "ECDHE-ECDSA-AES256-GCM-SHA384:"
        "ECDHE-RSA-AES256-GCM-SHA384:"
        "ECDHE-ECDSA-AES128-GCM-SHA256:"
        "ECDHE-RSA-AES128-GCM-SHA256:"
        "ECDHE-RSA-AES256-SHA384:"
        "AES256-GCM-SHA384";
    sslOpts.verify_peer = false;
    sslOpts.verify_host = false;
    // v16.10.12: ca_info is the single-file CA bundle
    // option (CURLOPT_CAINFO); ca_path is a *directory*
    // (CURLOPT_CAPATH) and libcurl was trying to fopen
    // the bundle file as a directory, which is the
    // `Error opening ca path ... ca-bundle.crt` the
    // field data showed.  ca_info is what we want for
    // a single Mozilla cacert.pem-style bundle.
    sslOpts.ca_info = HTTP::SSL_CA_PATH;
    session->SetSslOptions(sslOpts);
    return session;
}

int HTTP::prepareFetchSession(cpr::Session& session, const std::string& url) {
    session.SetTimeout(cpr::Timeout{HTTP::TIMEOUT});
    session.SetConnectTimeout(cpr::ConnectTimeout{HTTP::CONNECTION_TIMEOUT});
    session.SetHeader(HTTP::HEADERS);
    if (HTTP::hasProxy()) {
        session.SetProxies(HTTP::PROXIES);
    }
    session.SetVerifySsl(HTTP::VERIFY);
    // Same TLS 1.2 pin as createSession — Cloudflare + mbedTLS 2.28
    // handshake without this fails with -0x7780.
    cpr::SslOptions sslOpts;
    sslOpts.ssl_version = CURL_SSLVERSION_TLSv1_2;
    sslOpts.ciphers =
        "ECDHE-ECDSA-AES256-GCM-SHA384:"
        "ECDHE-RSA-AES256-GCM-SHA384:"
        "ECDHE-ECDSA-AES128-GCM-SHA256:"
        "ECDHE-RSA-AES128-GCM-SHA256:"
        "ECDHE-RSA-AES256-SHA384:"
        "AES256-GCM-SHA384";
    sslOpts.verify_peer = false;
    sslOpts.verify_host = false;
    sslOpts.ca_info = HTTP::SSL_CA_PATH;
    session.SetSslOptions(sslOpts);
    session.SetUrl(cpr::Url{url});  // keep hostname — SNI
#if defined(__SWITCH__)
    if (HTTP::hasProxy()) return 0;
    return HTTP::applyDnsToSession(session, url);
#else
    return 0;
#endif
}

#if defined(__SWITCH__)
namespace {
bool isFakeIp(const char* p) {
    unsigned int a = 0, b = 0;
    return p && sscanf(p, "%u.%u", &a, &b) == 2 && a == 198 &&
           (b == 18 || b == 19);
}

// Cloudflare anycast edges (DoH 2026-09-17). Correct SNI + these
// IPs works for ANY Cloudflare-fronted hostname, so a single pair
// covers static.myani.org / lain.bgm.tv / most image CDNs.
const char* kCfEdges[] = {"104.21.4.42", "172.67.131.164",
                          "104.21.3.123", "172.67.128.90"};

// DoH over a hardcoded resolver IP — no chicken-and-egg: URL host
// is the IP itself, which Cloudflare DoH accepts.
bool dohResolve(const std::string& host, std::string& outIp) {
    static const char* kDoh[] = {
        "https://1.1.1.1/dns-query",
        "https://8.8.8.8/resolve",
    };
    for (const char* base : kDoh) {
        try {
            cpr::Session s;
            s.SetTimeout(cpr::Timeout{4000});
            s.SetConnectTimeout(cpr::ConnectTimeout{2500});
            cpr::SslOptions ssl;
            ssl.ssl_version = CURL_SSLVERSION_TLSv1_2;
            ssl.ciphers =
                "ECDHE-ECDSA-AES256-GCM-SHA384:"
                "ECDHE-RSA-AES256-GCM-SHA384:"
                "ECDHE-ECDSA-AES128-GCM-SHA256:"
                "ECDHE-RSA-AES128-GCM-SHA256:"
                "AES256-GCM-SHA384";
            ssl.verify_peer = false;
            ssl.verify_host = false;
            ssl.ca_info = HTTP::SSL_CA_PATH;
            s.SetSslOptions(ssl);
            s.SetHeader(cpr::Header{{"Accept", "application/dns-json"},
                                    {"User-Agent", "ani-switch/0.1.0"}});
            // 8.8.8.8/resolve uses ?name=&type=A; 1.1.1.1 same shape.
            const std::string u =
                std::string(base) + "?name=" + host + "&type=A";
            s.SetUrl(cpr::Url{u});
            auto r = s.Get();
            if (r.error || r.status_code != 200 || r.text.empty()) continue;
            auto j = nlohmann::json::parse(r.text, nullptr, false);
            if (j.is_discarded() || !j.contains("Answer") ||
                !j["Answer"].is_array()) {
                continue;
            }
            for (const auto& ans : j["Answer"]) {
                if (!ans.contains("data") || !ans["data"].is_string()) continue;
                const std::string data = ans["data"].get<std::string>();
                // Skip CNAMEs / AAAA; want dotted-quad A.
                if (data.find('.') == std::string::npos) continue;
                if (data.find(':') != std::string::npos) continue;
                if (isFakeIp(data.c_str())) continue;
                unsigned int a = 0;
                if (sscanf(data.c_str(), "%u.", &a) != 1) continue;
                outIp = data;
                char _b[180];
                snprintf(_b, sizeof(_b), "HTTP: DoH %s -> %s (%s)",
                         host.c_str(), outIp.c_str(), base);
                aniswitchStartupLog(_b);
                return true;
            }
        } catch (...) {
            // try next resolver
        }
    }
    return false;
}
}  // namespace
#endif

int HTTP::resolveHostForUrl(const std::string& url,
                            std::string& outIp,
                            std::string& outHost) {
#if !defined(__SWITCH__)
    (void)url;
    (void)outIp;
    (void)outHost;
    return -1;
#else
    outIp.clear();
    outHost.clear();
    auto schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) return -1;
    auto hostStart = schemeEnd + 3;
    auto after = url.find_first_of("/?#", hostStart);
    outHost = url.substr(hostStart,
                         (after == std::string::npos) ? std::string::npos
                                                      : after - hostStart);
    if (outHost.empty()) return -1;

    static std::unordered_map<std::string, std::string> cache;

    auto accept = [&](const std::string& ip, const char* why) -> int {
        outIp = ip;
        cache[outHost] = ip;
        char _b[200];
        snprintf(_b, sizeof(_b), "HTTP: RESOLVE %s -> %s (%s)",
                 outHost.c_str(), ip.c_str(), why);
        aniswitchStartupLog(_b);
        return 0;
    };

    // 1) cache
    auto it = cache.find(outHost);
    if (it != cache.end()) {
        if (!isFakeIp(it->second.c_str())) {
            outIp = it->second;
            return 0;
        }
        cache.erase(it);
        aniswitchStartupLog("HTTP: RESOLVE cache had FAKE-IP, drop");
    }

    // 2) raw UDP DNS (aniswitchResolveHost, multi-nameserver)
    char ipBuf[32] = {0};
    if (aniswitchResolveHost(outHost.c_str(), ipBuf, sizeof(ipBuf)) == 0) {
        if (!isFakeIp(ipBuf)) return accept(ipBuf, "raw DNS");
        char _b[180];
        snprintf(_b, sizeof(_b),
                 "HTTP: RESOLVE %s -> %s FAKE-IP (Clash DNS?), reject",
                 outHost.c_str(), ipBuf);
        aniswitchStartupLog(_b);
    } else {
        char _b[160];
        snprintf(_b, sizeof(_b), "HTTP: RESOLVE %s FAILED (raw DNS)",
                 outHost.c_str());
        aniswitchStartupLog(_b);
    }

    // 3) DoH to hardcoded 1.1.1.1 / 8.8.8.8 (bypasses LAN DNS hijack)
    std::string dohIp;
    if (dohResolve(outHost, dohIp) && !dohIp.empty() && !isFakeIp(dohIp.c_str())) {
        return accept(dohIp, "doh");
    }

    // 4) Cloudflare anycast last-resort. Correct SNI + CF edge works
    // for every CF-fronted host (images, lain.bgm.tv, myani, …).
    for (const char* edge : kCfEdges) {
        if (isFakeIp(edge)) continue;
        return accept(edge, "cf-anycast");
    }
    return -1;
#endif
}

int HTTP::applyDnsToSession(cpr::Session& session, const std::string& url) {
#if !defined(__SWITCH__)
    (void)session;
    (void)url;
    return -1;
#else
    std::string ip, host;
    if (HTTP::resolveHostForUrl(url, ip, host) != 0 || ip.empty() ||
        host.empty()) {
        return -1;
    }
    session.SetResolve(cpr::Resolve{host, ip, {443U}});
    return 0;
#endif
}

int HTTP::rewriteUrlForIP(const std::string& url,
                          std::string& effectiveUrl,
                          std::string& hostForHeader) {
#if defined(__SWITCH__)
    // Kept for legacy callers. Prefer applyDnsToSession + original
    // URL so TLS SNI stays the hostname.
    std::string ip, host;
    effectiveUrl = url;
    hostForHeader.clear();
    if (HTTP::resolveHostForUrl(url, ip, host) != 0) return -1;
    auto schemeEnd = url.find("://");
    auto hostStart = (schemeEnd == std::string::npos) ? 0 : schemeEnd + 3;
    auto after = url.find_first_of("/?#", hostStart);
    std::string rest = (after == std::string::npos) ? "" : url.substr(after);
    effectiveUrl = url.substr(0, hostStart) + ip + rest;
    hostForHeader = host;
    return 0;
#else
    (void)url;
    return -1;
#endif
}

void HTTP::_cpr_post(const std::string& url,
                     const cpr::Parameters& parameters,
                     const cpr::Payload& payload,
                     std::function<void(const cpr::Response&)> cb,
                     ErrorCallback err) {
#if defined(__SWITCH__)
    // v19.0.4: case B' activation.  When the settings UI (or a
    // field-test build) flips HTTP::USE_DIRECT_TLS, route this
    // POST through the mbedTLS-direct path instead of cpr/curl.
    // The cpr/curl path stays exactly as it was below — this
    // branch only fires when USE_DIRECT_TLS is true.
    if (HTTP::USE_DIRECT_TLS) {
        directCallBypass(url, parameters, payload, "POST",
                         std::move(cb), std::move(err), HTTP::TIMEOUT);
        return;
    }
#endif
    // v16.10.2: switched from `session->PostCallback(...)` to
    // synchronous `session->Post()`.  PostCallback routes the
    // request through cpr's GlobalThreadPool, which is a
    // `new std::thread(...)`-backed worker (cpr/threadpool.cpp:73).
    // On Switch newlib, those worker threads hang in
    // curl_easy_perform — the user sees the loading spinner
    // forever, the success/error callback never fires.
    // Synchronous Post() runs on the caller's thread, so any
    // network failure (timeout / SSL / DNS / socket) returns
    // normally and the callback chain runs to completion.
    // The trade-off is that the calling UI fragment blocks
    // for up to HTTP::TIMEOUT ms while the request is in
    // flight; v15+ keeps TIMEOUT at 15s.
    brls::Logger::info("HTTP: POST {}", url);
#if defined(__SWITCH__)
    { char _b[160]; snprintf(_b, sizeof(_b), "HTTP: POST %s", url.c_str()); aniswitchStartupLog(_b); }
#endif
    auto session = createSession();
#if defined(__SWITCH__)
    // v22: shared resolve (fake-ip reject + DoH + CF anycast).
    // URL keeps hostname so SNI stays correct.
    if (!HTTP::hasProxy()) {
        if (HTTP::applyDnsToSession(*session, url) != 0) {
            aniswitchStartupLog("HTTP: POST resolve failed, proceed as-is");
        }
    } else {
        aniswitchStartupLog("HTTP: skip raw DNS (proxy active)");
    }
#endif
    session->SetUrl(cpr::Url{url});
    session->SetParameters(parameters);
    session->SetPayload(payload);
    cpr::Response r = session->Post();
    brls::Logger::info("HTTP: POST {} -> status={} error={}", url, r.status_code,
                       r.error ? r.error.message : "(none)");
#if defined(__SWITCH__)
    httpLog("HTTP: POST %s -> status=%d error=%s", url, r.status_code,
            r.error ? r.error.message.c_str() : "");
#endif
    if (r.error) {
        fireError(err, r.error.message, -1);
        return;
    }
    if (r.status_code != 200 && r.status_code != 204) {
        fireError(err, std::string("Network error. [") + std::to_string(r.status_code) + "]", r.status_code);
        return;
    }
    cb(r);
}

void HTTP::_cpr_get(const std::string& url,
                    const cpr::Parameters& parameters,
                    std::function<void(const cpr::Response&)> cb,
                    ErrorCallback err) {
#if defined(__SWITCH__)
    // v19.0.4: case B' activation — see _cpr_post above.  GET
    // has no POST body, so the payload is an empty cpr::Payload{}
    // (CurlContainer<Pair>::GetContent returns "" for an empty
    // containerList_, which the helper drops because method !=
    // "POST").
    if (HTTP::USE_DIRECT_TLS) {
        directCallBypass(url, parameters, cpr::Payload{}, "GET",
                         std::move(cb), std::move(err), HTTP::TIMEOUT);
        return;
    }
#endif
    // See _cpr_post above — v16.10.2 uses synchronous `session->Get()`
    // so the network round trip runs on the calling (UI) thread
    // instead of cpr's std::thread-backed GlobalThreadPool, which
    // hangs on Switch newlib.
    brls::Logger::info("HTTP: GET {}", url);
#if defined(__SWITCH__)
    { char _b[160]; snprintf(_b, sizeof(_b), "HTTP: GET %s", url.c_str()); aniswitchStartupLog(_b); }
#endif
    auto session = createSession();
#if defined(__SWITCH__)
    // v22: shared resolve (fake-ip reject + DoH + CF anycast).
    if (!HTTP::hasProxy()) {
        if (HTTP::applyDnsToSession(*session, url) != 0) {
            aniswitchStartupLog("HTTP: GET resolve failed, proceed as-is");
        }
    } else {
        aniswitchStartupLog("HTTP: skip raw DNS (proxy active)");
    }
#endif
    session->SetUrl(cpr::Url{url});
    session->SetParameters(parameters);
    cpr::Response r = session->Get();
    brls::Logger::info("HTTP: GET {} -> status={} error={}", url, r.status_code,
                       r.error ? r.error.message : "(none)");
#if defined(__SWITCH__)
    httpLog("HTTP: GET %s -> status=%d error=%s", url, r.status_code,
            r.error ? r.error.message.c_str() : "");
#endif
    if (r.error) {
        fireError(err, r.error.message, -1);
        return;
    }
    if (r.status_code != 200 && r.status_code != 204) {
        fireError(err, std::string("Network error. [") + std::to_string(r.status_code) + "]", r.status_code);
        return;
    }
#if defined(__SWITCH__)
    aniswitchStartupLog("HTTP: GET body ok, parse...");
#endif
    cb(r);
}

void HTTP::postVoid(const std::string& url,
                    cpr::Parameters parameters,
                    cpr::Payload payload,
                    std::function<void()> cb,
                    ErrorCallback err) {
    _cpr_post(url, parameters, payload,
              [cb, err](const cpr::Response& r) {
                  if (r.status_code == 204 || r.text.empty()) {
                      if (cb) cb();
                      return;
                  }
                  try {
                      auto j = nlohmann::json::parse(r.text);
                      int code = j.contains("code") && j["code"].is_number_integer()
                                ? j["code"].get<int>() : 0;
                      if (code == 0) {
                          if (cb) cb();
                          return;
                      }
                      std::string msg = j.contains("message") && j["message"].is_string()
                                ? j["message"].get<std::string>() : "unknown";
                      fireError(err, msg, code);
                  } catch (const std::exception& e) {
                      fireError(err, e.what(), r.status_code);
                  }
              },
              err);
}

}  // namespace aniswitch

// v19.0.2 case B' skeleton: HTTP::directFetch implementation.
// Lives below the closing namespace so the change doesn't touch
// the case A patch range (lines 21-49 of this file) and so
// non-Switch builds still link cleanly.

#if defined(__SWITCH__)
// v19.0.2 case B': include the mbedTLS direct path.  Pulled in
// only on Switch so non-Switch desktop / unit-test builds do not
// need mbedTLS headers on their include path.
#include "net/mbedtls_direct.hpp"
namespace aniswitch {

DirectFetchResult HTTP::directFetch(
        const std::string& url,
        const std::string& method,
        const std::string& body,
        const std::vector<std::string>& headers,
        long timeout_ms) {
    DirectFetchResult out;
    if (!HTTP::USE_DIRECT_TLS) {
        out.error = "direct TLS disabled (set HTTP::USE_DIRECT_TLS = true)";
        return out;
    }
    mbedtls_direct::DirectTLSConfig cfg;
    cfg.ca_bundle_path = HTTP::SSL_CA_PATH;
    cfg.verify_peer    = static_cast<bool>(HTTP::VERIFY);
    cfg.timeout_ms     = static_cast<int>(timeout_ms);
    cfg.port           = 443;
    auto r = mbedtls_direct::fetch_https(url, cfg, method, body, headers);
    out.status     = static_cast<long>(r.status);
    out.body       = std::move(r.body);
    out.error      = std::move(r.error);
    out.elapsed_ms = r.elapsed_ms;
    return out;
}

}  // namespace aniswitch
#endif


