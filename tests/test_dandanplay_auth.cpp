// SPDX-License-Identifier: AGPL-3.0
//
// Unit tests for dandanplay v2 request signing (matches open-ani/animeko's
// algorithm in danmaku/dandanplay/DandanplayClient.kt).
//
// We test the production dandanplay_auth.{hpp,cpp} via its public header
// rather than re-implementing the algorithm here, so a divergence between
// the algorithm and the production code is caught.
//
// The production code uses mbedtls_sha256_ret (linked on Switch at NRO
// build time).  On desktop hosts without mbedTLS, we link against a
// local portable SHA-256 via tests/sha256.h + tests/mbedtls/sha256.h
// (a shim that exposes the mbedtls API but is implemented in plain C).
// Algorithm equality is verified by running both against the same
// RFC 4648 and NIST FIPS 180-2 known vectors.

#include "net/dandanplay_auth.hpp"
#include <cstdio>
#include <string>

namespace dpauth = aniswitch::dandanplay;

#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL: %s @ %d\n", #cond, __LINE__); \
        std::exit(1); \
    } \
} while (0)

#define EXPECT_EQ(a, b) do { \
    auto av = (a); auto bv = (b); \
    if (av != bv) { \
        std::fprintf(stderr, "FAIL: %s == %s @ %d\n", \
                     #a, #b, __LINE__); \
        std::exit(1); \
    } \
} while (0)

int main() {
    using namespace dpauth;

    // ---- base64Std: RFC 4648 §10 known vectors ----
    EXPECT_EQ(base64Std(reinterpret_cast<const unsigned char*>(""), 0), "");
    EXPECT_EQ(base64Std(reinterpret_cast<const unsigned char*>("f"), 1), "Zg==");
    EXPECT_EQ(base64Std(reinterpret_cast<const unsigned char*>("fo"), 2), "Zm8=");
    EXPECT_EQ(base64Std(reinterpret_cast<const unsigned char*>("foo"), 3), "Zm9v");
    EXPECT_EQ(base64Std(reinterpret_cast<const unsigned char*>("foob"), 4), "Zm9vYg==");
    EXPECT_EQ(base64Std(reinterpret_cast<const unsigned char*>("fooba"), 5), "Zm9vYmE=");
    EXPECT_EQ(base64Std(reinterpret_cast<const unsigned char*>("foobar"), 6), "Zm9vYmFy");

    // ---- urlPath ----
    EXPECT_EQ(urlPath("https://api.dandanplay.net/api/v2/match"),
              "/api/v2/match");
    EXPECT_EQ(urlPath("https://api.dandanplay.net/api/v2/match?x=1&y=2"),
              "/api/v2/match");
    EXPECT_EQ(urlPath("http://localhost:8080/foo"), "/foo");
    EXPECT_EQ(urlPath("https://api.dandanplay.net"), "/");
    EXPECT_EQ(urlPath("/relative/path?q=1"), "/relative/path");

    // ---- sha256Base64 against NIST FIPS 180-2 §B.1 vector ----
    // SHA-256("abc") =
    //   ba7816bf 8f01cfea 414140de 5dae2223 b00361a3 96177a9c b410ff61 f20015ad
    //   (this is the canonical value reproduced by sha256.cpp above)
    // Base64 of that = "ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0="
    EXPECT_EQ(sha256Base64("abc"),
              "ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0=");

    // SHA-256 of the 56-byte multi-block FIPS test vector
    //   "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
    //   = 248d6a61 d20638b8 e5c02693 0c3e6039 a33ce459 64ff2167 f6ecedd4 19db06c1
    //   (reproduced by sha256.cpp above; this hash is the canonical
    //    NIST FIPS 180-2 §B.2 example)
    // Base64 of that = "JI1qYdIGOLjlwCaTDD5gOaM85Flk/yFn9uzt1BnbBsE="
    EXPECT_EQ(sha256Base64("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
              "JI1qYdIGOLjlwCaTDD5gOaM85Flk/yFn9uzt1BnbBsE=");

    // ---- sign() against a hand-computed dandanplay vector ----
    // Signature = Base64( SHA-256( appId || timestamp || path || appSecret ) ).
    // For appId="test_app", ts=1700000000, path="/api/v2/match",
    // appSecret="secret", the canonical input is
    //   "test_app1700000000/api/v2/matchsecret"
    // whose SHA-256 is
    //   35f3c818 4a912495 9a603d64 f08cf4d7 62e5023c 525ece4d 681ef3a1 38d45012
    // and Base64 of that is "NfPIGEqRJJWaYD1k8Iz012LlAjxSXs5NaB7zoTjUUBI="
    // (verified with the diagnostics test_sign_diag).
    EXPECT_EQ(sign("test_app", 1700000000, "/api/v2/match", "secret"),
              "NfPIGEqRJJWaYD1k8Iz012LlAjxSXs5NaB7zoTjUUBI=");

    // Distinctness — different inputs must produce different signatures.
    EXPECT(sign("test_app", 1700000000, "/api/v2/match", "other") !=
           sign("test_app", 1700000000, "/api/v2/match", "secret"));
    EXPECT(sign("test_app", 1700000000, "/api/v2/comment/123", "secret") !=
           sign("test_app", 1700000000, "/api/v2/match",       "secret"));
    EXPECT(sign("test_app", 1700000001, "/api/v2/match", "secret") !=
           sign("test_app", 1700000000, "/api/v2/match", "secret"));
    EXPECT(sign("other",   1700000000, "/api/v2/match", "secret") !=
           sign("test_app", 1700000000, "/api/v2/match", "secret"));

    std::puts("test_dandanplay_auth: OK");
    return 0;
}
