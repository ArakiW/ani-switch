// SPDX-License-Identifier: CC0-1.0
//
// Minimal SHA-256 (FIPS 180-2) implementation for unit tests.  Mirrors
// the mbedtls_sha256_ret() calling convention so the dandanplay_auth
// code is testable on systems without mbedTLS (e.g. desktop CI).
// NOT a production crypto primitive — the production code path uses
// mbedtls_sha256_ret from the devkitpro Switch SDK at link time.
//
// Algorithm reference: FIPS PUB 180-2 §6.2.

#pragma once

#include <cstdint>
#include <cstddef>

namespace test_sha256 {

// Always 32 bytes for SHA-256.
constexpr std::size_t DIGEST_SIZE = 32;

struct Context {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  buffer[64];
    std::size_t buflen;
};

void init(Context& ctx);
void update(Context& ctx, const uint8_t* data, std::size_t len);
void final_(Context& ctx, uint8_t out[DIGEST_SIZE]);

// Convenience: one-shot hash.  Equivalent to init+update+final_.
inline void hash(const uint8_t* data, std::size_t len, uint8_t out[DIGEST_SIZE]) {
    Context ctx;
    init(ctx);
    update(ctx, data, len);
    final_(ctx, out);
}

}  // namespace test_sha256

// shim: anything that does
//   #include "sha256.h"
// outside the test_sha256 namespace gets a mbedtls-style API.  The
// dandanplay_auth.cpp also uses this shim when compiled into a test
// target (the test CMakeLists links either mbedTLS or this header).

struct mbedtls_sha256_context {
    test_sha256::Context ctx;
};

#ifdef __cplusplus
extern "C" {
#endif

inline int mbedtls_sha256_starts(mbedtls_sha256_context* ctx, int /*is224*/) {
    test_sha256::init(ctx->ctx);
    return 0;
}

inline int mbedtls_sha256_update(mbedtls_sha256_context* ctx,
                                 const unsigned char* data, std::size_t len) {
    test_sha256::update(ctx->ctx, data, len);
    return 0;
}

inline int mbedtls_sha256_finish(mbedtls_sha256_context* ctx, unsigned char out[32]) {
    test_sha256::final_(ctx->ctx, out);
    return 0;
}

inline int mbedtls_sha256_ret(const unsigned char* data, std::size_t len,
                              unsigned char out[32], int /*is224*/) {
    test_sha256::Context ctx;
    test_sha256::init(ctx);
    test_sha256::update(ctx, data, len);
    test_sha256::final_(ctx, out);
    return 0;
}

#ifdef __cplusplus
}
#endif
