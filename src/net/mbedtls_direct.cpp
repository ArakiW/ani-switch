// SPDX-License-Identifier: AGPL-3.0
//
// v19.0.2 case B' skeleton implementation.  See mbedtls_direct.hpp
// for the design rationale.

#include "net/mbedtls_direct.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <utility>

#if defined(__SWITCH__)
extern "C" int  aniswitchResolveHost(const char* host, char* out_ip, size_t out_len);
extern "C" void aniswitchStartupLog(const char* message);
#endif

namespace aniswitch {
namespace mbedtls_direct {

#if defined(__SWITCH__)

// ---- helpers --------------------------------------------------------

static void mbed_err(int rc, std::string* error_out) {
    if (!error_out) return;
    char buf[256] = {0};
    mbedtls_strerror(rc, buf, sizeof(buf) - 1);
    char out[300];
    std::snprintf(out, sizeof(out), "mbedTLS rc=%d: %s", rc, buf);
    *error_out = out;
}

static void startup_log(const char* s) {
#if defined(__SWITCH__)
    if (s) aniswitchStartupLog(s);
#else
    (void)s;
#endif
}

// Ciphersuite list (4 explicit entries, NULL-terminated).
// Preference order matches docs/v19.0.2-mbedtls-direct.hpp — first
// matching suite wins.
static const int kCiphersuites[] = {
    MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
    MBEDTLS_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
    MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
    MBEDTLS_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
    0
};

// Signature-algorithm list (5 entries, MBEDTLS_MD_NONE terminator).
// Excludes SHA-1 (RFC 9155).  Order = decreasing preference.
static const int kSigHashes[] = {
    MBEDTLS_MD_SHA512,
    MBEDTLS_MD_SHA384,
    MBEDTLS_MD_SHA256,
    MBEDTLS_MD_SHA224,
    MBEDTLS_MD_NONE
};

// ---- class ----------------------------------------------------------

DirectTLS::DirectTLS() {
    mbedtls_ssl_init(&ssl_);
    mbedtls_ssl_config_init(&conf_);
    mbedtls_ctr_drbg_init(&ctr_drbg_);
    mbedtls_entropy_init(&entropy_);
    mbedtls_x509_crt_init(&cacert_);
    mbedtls_net_init(&net_);
}

DirectTLS::~DirectTLS() {
    close();
}

int DirectTLS::init(const DirectTLSConfig& cfg, std::string* error_out) {
    if (inited_) return 0;
    ca_bundle_path_ = cfg.ca_bundle_path;
    verify_peer_    = cfg.verify_peer;
    int rc;

    // 1. Seed RNG.
    rc = mbedtls_ctr_drbg_seed(
        &ctr_drbg_, mbedtls_entropy_func, &entropy_,
        reinterpret_cast<const unsigned char*>("aniswitch-directtls"), 19);
    if (rc != 0) { mbed_err(rc, error_out); return rc; }

    // 2. Load CA bundle (Mozilla cacert.pem style).  If the file is
    //    missing or unreadable we still proceed — the handshake
    //    will fail later if verify_peer is on, but that's a
    //    runtime issue, not an init issue.
    if (!cfg.ca_bundle_path.empty()) {
        rc = mbedtls_x509_crt_parse_file(&cacert_, cfg.ca_bundle_path.c_str());
        if (rc < 0) {
            char log[200];
            std::snprintf(log, sizeof(log),
                          "MBEDTLS_DIRECT: ca-bundle parse rc=%d (path=%s, continuing)",
                          rc, cfg.ca_bundle_path.c_str());
            startup_log(log);
            rc = 0;  // mbedtls_x509_crt_parse_file leaves cacert_ in
                     // an undefined state on negative return; resetting
                     // rc=0 means we fall through to ssl_config_defaults
                     // on a fresh conf_, and the verify chain will
                     // simply fail at handshake time (the right
                     // behaviour given verify_peer may still be on).
        } else if (rc > 0) {
            char log[80];
            std::snprintf(log, sizeof(log),
                          "MBEDTLS_DIRECT: %d warnings parsing CA bundle", rc);
            startup_log(log);
            rc = 0;
        }
    }

    // 3. Default SSL config (TLS 1.2 client).
    rc = mbedtls_ssl_config_defaults(
        &conf_, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT);
    if (rc != 0) { mbed_err(rc, error_out); return rc; }

    mbedtls_ssl_conf_authmode(&conf_,
                              verify_peer_ ? MBEDTLS_SSL_VERIFY_REQUIRED
                                           : MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_ca_chain(&conf_, &cacert_, nullptr);
    mbedtls_ssl_conf_ciphersuites(&conf_, kCiphersuites);
    mbedtls_ssl_conf_sig_hashes(&conf_, kSigHashes);
    mbedtls_ssl_conf_rng(&conf_, mbedtls_ctr_drbg_random, &ctr_drbg_);

    // Pin TLS 1.2 only.  mbedTLS 2.28.10 does not implement TLS
    // 1.3 at all (the protocol was added in 3.x), so the
    // cpr/curl/mbedTLS path on Switch is TLS-1.2-only by
    // construction.  The explicit pin here is belt-and-braces:
    // if a future 2.28 backport adds 1.3 we'll still refuse.
    // The mbedTLS 2.28 API does not have mbedtls_ssl_conf_groups()
    // (that's 3.x), so we don't pin curve lists — mbedTLS's
    // defaults (secp256r1 / secp384r1 / x25519) are fine for ECDHE.
    mbedtls_ssl_conf_min_version(&conf_, MBEDTLS_SSL_MAJOR_VERSION_3,
                                 MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_max_version(&conf_, MBEDTLS_SSL_MAJOR_VERSION_3,
                                 MBEDTLS_SSL_MINOR_VERSION_3);

    inited_ = true;
    return 0;
}

int DirectTLS::connect(const std::string& host, std::string* error_out) {
    if (!inited_) {
        if (error_out) *error_out = "DirectTLS::init() not called";
        return -1;
    }
    if (connected_) {
        if (error_out) *error_out = "DirectTLS already connected";
        return -1;
    }
    hostname_ = host;

    // 1. Resolve host → IP via the platform's raw UDP DNS path.
    //    Passing the IP literal to mbedtls_net_connect() skips
    //    newlib's gethostbyname, which is what hangs in hbmenu
    //    applet mode.
    char ipBuf[32] = {0};
#if defined(__SWITCH__)
    if (aniswitchResolveHost(host.c_str(), ipBuf, sizeof(ipBuf)) != 0) {
        if (error_out) *error_out = "DNS resolve failed for " + host;
        return -1;
    }
#else
    if (error_out) *error_out = "DirectTLS::connect only on __SWITCH__";
    return -1;
#endif

    char portStr[8];
    std::snprintf(portStr, sizeof(portStr), "%d", 443);

    // 2. TCP connect.
    int rc = mbedtls_net_connect(&net_, ipBuf, portStr, MBEDTLS_NET_PROTO_TCP);
    if (rc != 0) { mbed_err(rc, error_out); return rc; }

    // 3. SSL setup + SNI hostname.  mbedtls_ssl_set_hostname must
    //    be called AFTER mbedtls_ssl_setup.
    rc = mbedtls_ssl_setup(&ssl_, &conf_);
    if (rc != 0) { mbed_err(rc, error_out); mbedtls_net_free(&net_); return rc; }
    rc = mbedtls_ssl_set_hostname(&ssl_, host.c_str());
    if (rc != 0) {
        mbed_err(rc, error_out);
        mbedtls_ssl_free(&ssl_);
        mbedtls_net_free(&net_);
        return rc;
    }
    mbedtls_ssl_set_bio(&ssl_, &net_,
                        mbedtls_net_send, mbedtls_net_recv,
                        mbedtls_net_recv_timeout);

    // 4. TLS handshake (loop on WANT_READ/WANT_WRITE).
    while ((rc = mbedtls_ssl_handshake(&ssl_)) != 0) {
        if (rc != MBEDTLS_ERR_SSL_WANT_READ &&
            rc != MBEDTLS_ERR_SSL_WANT_WRITE) {
            mbed_err(rc, error_out);
            mbedtls_ssl_free(&ssl_);
            mbedtls_net_free(&net_);
            return rc;
        }
    }

    connected_ = true;
    return 0;
}

int DirectTLS::send(const std::string& wire, std::string* error_out) {
    if (!connected_) {
        if (error_out) *error_out = "DirectTLS not connected";
        return -1;
    }
    size_t total = 0;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(wire.data());
    size_t remaining = wire.size();
    while (remaining > 0) {
        int n = mbedtls_ssl_write(&ssl_, p, remaining);
        if (n < 0) {
            if (n == MBEDTLS_ERR_SSL_WANT_READ ||
                n == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
            mbed_err(n, error_out);
            return n;
        }
        if (n == 0) break;
        p         += n;
        remaining -= static_cast<size_t>(n);
        total     += static_cast<size_t>(n);
    }
    return static_cast<int>(total);
}

int DirectTLS::recv(void* out, int len, std::string* error_out) {
    if (!connected_) {
        if (error_out) *error_out = "DirectTLS not connected";
        return -1;
    }
    if (len <= 0) return 0;
    int n;
    do {
        n = mbedtls_ssl_read(&ssl_, static_cast<unsigned char*>(out),
                             static_cast<size_t>(len));
    } while (n == MBEDTLS_ERR_SSL_WANT_READ ||
             n == MBEDTLS_ERR_SSL_WANT_WRITE);
    if (n < 0) mbed_err(n, error_out);
    return n;
}

void DirectTLS::close() {
    if (connected_) {
        mbedtls_ssl_close_notify(&ssl_);
    }
    mbedtls_net_free(&net_);
    mbedtls_x509_crt_free(&cacert_);
    mbedtls_ssl_free(&ssl_);
    mbedtls_ssl_config_free(&conf_);
    mbedtls_ctr_drbg_free(&ctr_drbg_);
    mbedtls_entropy_free(&entropy_);
    // Re-init so the object is reusable.
    mbedtls_net_init(&net_);
    mbedtls_x509_crt_init(&cacert_);
    mbedtls_ssl_init(&ssl_);
    mbedtls_ssl_config_init(&conf_);
    mbedtls_ctr_drbg_init(&ctr_drbg_);
    mbedtls_entropy_init(&entropy_);
    inited_    = false;
    connected_ = false;
}

// ---- HTTP/1.1 response parser --------------------------------------

// Parses "HTTP/1.1 200 OK\r\nHeaders...\r\n\r\nBody" into status,
// headers, body.  Returns 0 on success, -1 on parse error.
static int parse_http_response(
        const std::string& raw,
        int* status_out,
        std::vector<std::pair<std::string, std::string>>* headers_out,
        std::string* body_out) {
    size_t eol = raw.find("\r\n");
    if (eol == std::string::npos) return -1;
    std::string status_line = raw.substr(0, eol);
    size_t sp1 = status_line.find(' ');
    size_t sp2 = (sp1 == std::string::npos) ? std::string::npos
                                            : status_line.find(' ', sp1 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos) return -1;
    *status_out = std::atoi(status_line.substr(sp1 + 1, sp2 - sp1 - 1).c_str());

    size_t hdr_start = eol + 2;
    size_t hdr_end   = raw.find("\r\n\r\n", hdr_start);
    if (hdr_end == std::string::npos) return -1;
    std::string headers_block = raw.substr(hdr_start, hdr_end - hdr_start);

    size_t pos = 0;
    while (pos < headers_block.size()) {
        size_t eol2 = headers_block.find("\r\n", pos);
        if (eol2 == std::string::npos) eol2 = headers_block.size();
        std::string line = headers_block.substr(pos, eol2 - pos);
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string name  = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            size_t vs = value.find_first_not_of(" \t");
            if (vs != std::string::npos) value = value.substr(vs);
            headers_out->push_back({name, value});
        }
        if (eol2 == headers_block.size()) break;
        pos = eol2 + 2;
    }
    *body_out = raw.substr(hdr_end + 4);
    return 0;
}

// Build a wire-format HTTP/1.1 request with sane defaults.
static std::string build_http1_request(
        const std::string& method,
        const std::string& host,
        const std::string& path,
        const std::vector<std::string>& headers,
        const std::string& body) {
    std::string req;
    req.reserve(256 + body.size());
    req += method;
    req += ' ';
    req += path.empty() ? std::string("/") : path;
    req += " HTTP/1.1\r\nHost: ";
    req += host;
    req += "\r\n";
    bool has_user_agent = false, has_accept = false, has_conn = false,
         has_clen = false, has_ctype = false;
    for (const auto& h : headers) {
        if (h.rfind("User-Agent:",     0) == 0) has_user_agent = true;
        if (h.rfind("Accept:",         0) == 0) has_accept     = true;
        if (h.rfind("Connection:",     0) == 0) has_conn       = true;
        if (h.rfind("Content-Length:", 0) == 0) has_clen       = true;
        if (h.rfind("Content-Type:",   0) == 0) has_ctype      = true;
        req += h;
        req += "\r\n";
    }
    if (!has_user_agent) req += "User-Agent: ani-switch/0.1.0 (https://github.com/MiniMax139102/ani-switch)\r\n";
    if (!has_accept)     req += "Accept: application/json\r\n";
    if (!has_conn)       req += "Connection: close\r\n";
    if (!body.empty()) {
        if (!has_clen) {
            char buf[40];
            std::snprintf(buf, sizeof(buf),
                          "Content-Length: %zu\r\n", body.size());
            req += buf;
        }
        if (!has_ctype) {
            req += "Content-Type: application/x-www-form-urlencoded\r\n";
        }
    }
    req += "\r\n";
    req += body;
    return req;
}

DirectTLSResult fetch_https(const std::string& url,
                            const DirectTLSConfig& cfg,
                            const std::string& method,
                            const std::string& body,
                            const std::vector<std::string>& headers) {
    DirectTLSResult r;
    auto t0 = std::chrono::steady_clock::now();

    // Parse URL.
    size_t s = url.find("://");
    if (s == std::string::npos) { r.error = "bad URL: " + url; return r; }
    size_t hostStart = s + 3;
    size_t hostEnd   = url.find_first_of("/?#", hostStart);
    std::string host = url.substr(hostStart,
        (hostEnd == std::string::npos) ? std::string::npos
                                       : hostEnd - hostStart);
    if (host.empty()) { r.error = "empty host in " + url; return r; }
    size_t colon = host.find(':');
    if (colon != std::string::npos) host = host.substr(0, colon);
    std::string path = (hostEnd == std::string::npos) ? "/" : url.substr(hostEnd);

    DirectTLS tls;
    std::string err;
    if (int rc = tls.init(cfg, &err); rc != 0) {
        r.error = "init failed: " + err;
        return r;
    }
    if (int rc = tls.connect(host, &err); rc != 0) {
        r.error = "connect failed: " + err;
        return r;
    }
    std::string wire = build_http1_request(method, host, path, headers, body);
    if (int rc = tls.send(wire, &err); rc < 0) {
        r.error = "send failed: " + err;
        tls.close();
        return r;
    }
    // Read until clean EOF (Connection: close).
    std::string raw;
    char buf[4096];
    for (;;) {
        int n = tls.recv(buf, sizeof(buf), &err);
        if (n < 0) { r.error = "recv failed: " + err; tls.close(); return r; }
        if (n == 0) break;
        raw.append(buf, static_cast<size_t>(n));
    }
    tls.close();

    int status = 0;
    std::vector<std::pair<std::string, std::string>> respHeaders;
    std::string respBody;
    if (parse_http_response(raw, &status, &respHeaders, &respBody) != 0) {
        r.error = "parse failed (raw size=" + std::to_string(raw.size()) + ")";
        return r;
    }
    r.status = status;
    r.body   = respBody;
    auto t1 = std::chrono::steady_clock::now();
    r.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return r;
}

#else  // !__SWITCH__

// Non-Switch stub so the symbol exists for non-Switch builds (e.g.
// unit tests on a Linux dev box).  Always returns an error.
DirectTLSResult fetch_https(const std::string& url,
                            const DirectTLSConfig&,
                            const std::string&,
                            const std::string&,
                            const std::vector<std::string>&) {
    DirectTLSResult r;
    r.error = "fetch_https only on __SWITCH__ (got url=" + url + ")";
    return r;
}

#endif

}  // namespace mbedtls_direct
}  // namespace aniswitch
