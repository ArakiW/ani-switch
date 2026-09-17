// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
#include "net/uuid.hpp"
#include <random>
#include <cstdio>

namespace aniswitch {

std::string newUuid() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    uint64_t a = rng();
    uint64_t b = rng();
    // Set version (4) and variant (10) bits per RFC 4122
    a = (a & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    b = (b & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;
    char buf[37];
    std::snprintf(buf, sizeof(buf),
                  "%08x-%04x-%04x-%04x-%012llx",
                  static_cast<uint32_t>(a >> 32),
                  static_cast<uint32_t>((a >> 16) & 0xFFFF),
                  static_cast<uint32_t>(a & 0xFFFF),
                  static_cast<uint32_t>(b >> 48),
                  static_cast<unsigned long long>(b & 0xFFFFFFFFFFFFULL));
    return std::string(buf, 36);
}

}  // namespace aniswitch
