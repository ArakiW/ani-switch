// SPDX-License-Identifier: AGPL-3.0
//
// dandanplay v2 request signing helpers.  These live in their own header
// so the signature math can be unit-tested without pulling cpr.
//
// Algorithm (matches open-ani/animeko's DandanplayClient):
//   signature = Base64( SHA-256( appId + timestamp + path + appSecret ) )
// where `path` is the URL-encoded request path (no scheme, no host, no
// query string).  dandanplay treats the signature as a shared secret
// hash that the server recomputes on the same inputs.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace aniswitch::dandanplay {

// Standard Base64 (not URL-safe).  Padding with '=' on the tail.
// 0 on success, non-zero on error.
std::string base64Std(const unsigned char* data, std::size_t len);

// Current unix time in whole seconds.  dandanplay's X-Timestamp header
// wants the integer second, not milliseconds.
int64_t nowSeconds();

// SHA-256 of `in` then standard Base64.  Returns "" on internal error.
std::string sha256Base64(const std::string& in);

// Full dandanplay signature: SHA-256 + Base64 over
//   appId + timestamp + path + appSecret
// Returns the Base64 string suitable for the X-Signature header.
std::string sign(const std::string& appId, int64_t timestamp,
                 const std::string& path, const std::string& appSecret);

// Extract the URL path from a full URL.  e.g.
//   urlPath("https://api.dandanplay.net/api/v2/match?x=y")
//   == "/api/v2/match"
std::string urlPath(const std::string& url);

}  // namespace aniswitch::dandanplay
