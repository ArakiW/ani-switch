// SPDX-License-Identifier: AGPL-3.0

#include "net/dandanplay_auth.hpp"
#include <chrono>
#include <mbedtls/sha256.h>

namespace aniswitch::dandanplay {

std::string base64Std(const unsigned char* data, std::size_t len) {
    static const char* tbl =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    std::size_t i = 0;
    while (i + 2 < len) {
        uint32_t v = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8) | data[i + 2];
        out += tbl[(v >> 18) & 0x3F];
        out += tbl[(v >> 12) & 0x3F];
        out += tbl[(v >>  6) & 0x3F];
        out += tbl[ v        & 0x3F];
        i += 3;
    }
    if (i < len) {
        uint32_t v = uint32_t(data[i]) << 16;
        if (i + 1 < len) v |= uint32_t(data[i + 1]) << 8;
        out += tbl[(v >> 18) & 0x3F];
        out += tbl[(v >> 12) & 0x3F];
        out += (i + 1 < len) ? tbl[(v >> 6) & 0x3F] : '=';
        out += '=';
    }
    return out;
}

int64_t nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string sha256Base64(const std::string& in) {
    unsigned char digest[32];
    const int rc = mbedtls_sha256_ret(
        reinterpret_cast<const unsigned char*>(in.data()),
        in.size(), digest, /*is224=*/0);
    if (rc != 0) return {};
    return base64Std(digest, sizeof(digest));
}

std::string sign(const std::string& appId, int64_t timestamp,
                 const std::string& path, const std::string& appSecret) {
    std::string data;
    data.reserve(appId.size() + 16 + path.size() + appSecret.size());
    data.append(appId);
    data.append(std::to_string(timestamp));
    data.append(path);
    data.append(appSecret);
    return sha256Base64(data);
}

std::string urlPath(const std::string& url) {
    const auto schemeEnd = url.find("://");
    const std::size_t hostStart = (schemeEnd == std::string::npos) ? 0 : schemeEnd + 3;
    const auto pathStart = url.find('/', hostStart);
    if (pathStart == std::string::npos) return "/";
    const auto queryStart = url.find('?', pathStart);
    if (queryStart == std::string::npos) return url.substr(pathStart);
    return url.substr(pathStart, queryStart - pathStart);
}

}  // namespace aniswitch::dandanplay
