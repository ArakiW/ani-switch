// SPDX-License-Identifier: AGPL-3.0
#include "sha256.h"
#include <cstdio>
#include <string>

int main() {
    using namespace test_sha256;
    const std::string data = "test_app1700000000/api/v2/matchsecret";
    unsigned char d[32];
    hash(reinterpret_cast<const uint8_t*>(data.data()), data.size(), d);
    std::printf("data = %s\n", data.c_str());
    std::printf("hex  = ");
    for (auto b : d) std::printf("%02x", b);
    std::printf("\n");
    // Base64
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string b64;
    for (size_t i = 0; i < 32; i += 3) {
        uint32_t v = (uint32_t(d[i]) << 16) | (i + 1 < 32 ? (uint32_t(d[i+1]) << 8) : 0) | (i + 2 < 32 ? d[i+2] : 0);
        b64 += tbl[(v >> 18) & 0x3F];
        b64 += tbl[(v >> 12) & 0x3F];
        b64 += (i + 1 < 32) ? tbl[(v >> 6) & 0x3F] : '=';
        b64 += (i + 2 < 32) ? tbl[v & 0x3F] : '=';
    }
    std::printf("b64  = %s\n", b64.c_str());
    return 0;
}
