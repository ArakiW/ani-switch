// SPDX-License-Identifier: AGPL-3.0
#include "sha256.h"
#include <cstdio>
#include <cstring>
#include <string>

namespace dpauth {
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
std::string sha256Base64(const std::string& in) {
    unsigned char d[32];
    test_sha256::hash(reinterpret_cast<const uint8_t*>(in.data()), in.size(), d);
    return base64Std(d, 32);
}
}

int main() {
    const char* in = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    std::printf("len=%zu\n", strlen(in));
    unsigned char d[32];
    test_sha256::Context ctx;
    test_sha256::init(ctx);
    test_sha256::update(ctx, reinterpret_cast<const uint8_t*>(in), strlen(in));
    test_sha256::final_(ctx, d);
    std::printf("hex: ");
    for (auto b : d) std::printf("%02x", b);
    std::printf("\n");
    std::printf("expect: 248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1\n");
    std::printf("base64: %s\n", dpauth::sha256Base64(in).c_str());
    return 0;
}
