// SPDX-License-Identifier: AGPL-3.0
//
// v19.0.2 case B' skeleton: direct mbedTLS 2.28 HTTP/1.1 client.
//
// Why this exists: vendored cpr/curl 8.4.0 on the Switch uses the
// devkitpro portlibs mbedTLS 2.28.10 as its TLS backend, but the
// backend does not implement CURLOPT_SSL_CTX_FUNCTION, hard-codes
// MBEDTLS_SSL_VERIFY_OPTIONAL, and ignores CURLOPT_SSL_CIPHER_LIST
// (always uses the mbedTLS default ciphersuite list).  When the
// v19.0.1 debug NRO captured a Cloudflare-side Connection Reset on
// api.bgm.tv mid-handshake, every fix we wanted to try (pin a
// ciphersuite, force verify=required + ca-bundle, disable
// signature_algorithms containing SHA-1) needed a control knob
// curl doesn't expose.  So we reimplement the small piece we need:
// HTTP/1.1 over mbedTLS 2.28, talking to one IP at a time, with
// the exact ciphersuite + sig-hash + hostname we want.
//
// Status: SKELETON.  Compiles, links, ships a working API surface.
// Not end-to-end runtime tested in this commit — the goal is to
// unblock the v19.0.3 worker that will wire the existing HTTP
// createSession() over to this path when case A is not enough.

#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#if defined(__SWITCH__)
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"
#endif

namespace aniswitch {
namespace mbedtls_direct {

// ---- Configuration (passed in by HTTP::directFetch) ----------------

struct DirectTLSConfig {
    std::string ca_bundle_path = "sdmc:/switch/aniswitch/ca-bundle.crt";
    bool        verify_peer    = true;   // case D fix: required
    int         timeout_ms     = 10000;  // total request timeout
    int         port           = 443;
};

// ---- Per-request response ------------------------------------------

struct DirectTLSResponse {
    int         status_code = 0;     // HTTP/1.1 status, 0 on transport error
    std::string body;                // entity-body (decoded if chunked)
    std::string error;               // human-readable mbedTLS error, "" on success
};

// ---- The "fetch this URL" free function ----------------------------
//
// Used by HTTP::directFetch.  Kept as a free function (not a static
// method) so it is straightforward to unit-test on a non-Switch
// build by stubbing aniswitchResolveHost().

struct DirectTLSResult {
    int         status      = 0;     // HTTP status, 0 on transport error
    std::string body;                // response body
    std::string error;               // "" on success
    double      elapsed_ms  = 0;     // wall clock
};

DirectTLSResult fetch_https(const std::string& url,
                            const DirectTLSConfig& cfg,
                            const std::string& method  = "GET",
                            const std::string& body    = "",
                            const std::vector<std::string>& headers = {});

// ---- Class-based API (the five-step skeleton) ---------------------
//
// Init    — RNG seed, CA bundle load, ciphersuite + sig-hash list,
//           SNI hostname, authmode.
// Connect — DNS via aniswitchResolveHost() → TCP connect via
//           mbedtls_net_connect on the IP literal → TLS handshake.
// Send    — write the wire-formatted HTTP/1.1 request through
//           mbedtls_ssl_write (loops over WANT_READ/WANT_WRITE).
// Recv    — read up to `len` bytes from the encrypted stream.
// Close   — send close_notify, free everything.

#if defined(__SWITCH__)
class DirectTLS {
public:
    DirectTLS();
    ~DirectTLS();

    DirectTLS(const DirectTLS&)            = delete;
    DirectTLS& operator=(const DirectTLS&) = delete;

    // Returns 0 on success, mbedTLS error on failure.
    // `error_out` (optional) gets a human-readable mbedtls_strerror.
    int init(const DirectTLSConfig& cfg, std::string* error_out = nullptr);

    // Resolves `host` via the platform's aniswitchResolveHost() C
    // entry point (which knows the hardcoded IP table + raw UDP DNS
    // resolver), opens a TCP socket, runs the TLS handshake.
    // Returns 0 on success, mbedTLS error on failure.
    int connect(const std::string& host, std::string* error_out = nullptr);

    // Send an HTTP/1.1 wire request.  `wire` is the fully-formatted
    // request line + headers + body.  Returns the number of
    // plaintext bytes consumed, or a negative mbedTLS error code.
    int send(const std::string& wire, std::string* error_out = nullptr);

    // Read up to `len` bytes from the encrypted stream into `out`.
    // Returns the bytes placed in `out`, 0 on clean EOF, or a
    // negative mbedTLS error code.
    int recv(void* out, int len, std::string* error_out = nullptr);

    // Send close_notify and tear everything down.  Idempotent.
    void close();

    bool is_connected() const { return connected_; }

private:
    mbedtls_ssl_context       ssl_;
    mbedtls_ssl_config        conf_;
    mbedtls_ctr_drbg_context  ctr_drbg_;
    mbedtls_entropy_context   entropy_;
    mbedtls_x509_crt          cacert_;
    mbedtls_net_context       net_;
    std::string               hostname_;
    std::string               ca_bundle_path_;
    bool                      verify_peer_ = true;
    bool                      inited_      = false;
    bool                      connected_   = false;
};
#else
// Non-Switch stub so unit tests on Linux can link this header
// without pulling mbedTLS in.
class DirectTLS {
public:
    DirectTLS()  = default;
    ~DirectTLS() = default;
    DirectTLS(const DirectTLS&)            = delete;
    DirectTLS& operator=(const DirectTLS&) = delete;
    int  init(const DirectTLSConfig&, std::string* = nullptr)    { return -1; }
    int  connect(const std::string&, std::string* = nullptr)     { return -1; }
    int  send(const std::string&, std::string* = nullptr)         { return -1; }
    int  recv(void*, int, std::string* = nullptr)                 { return -1; }
    void close()                                                  {}
    bool is_connected() const                                     { return false; }
};
#endif

}  // namespace mbedtls_direct
}  // namespace aniswitch
