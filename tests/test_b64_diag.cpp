// SPDX-License-Identifier: AGPL-3.0
#include "sha256.h"
#include <cstdio>
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
}

int main() {
    using namespace test_sha256;
    unsigned char out[DIGEST_SIZE];
    Context ctx;
    init(ctx);
    update(ctx, reinterpret_cast<const uint8_t*>("abc"), 3);
    final_(ctx, out);
    std::printf("hex   : ");
    for (auto b : out) std::printf("%02x", b);
    std::printf("\n");
    std::printf("base64: %s\n", dpauth::base64Std(out, DIGEST_SIZE).c_str());
    std::printf("expect: ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctDBtFOmD3e4=\n");
    return 0;
}
