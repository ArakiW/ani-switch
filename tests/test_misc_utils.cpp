// SPDX-License-Identifier: AGPL-3.0
//
// Smoke test for the leaf utility helpers (string, number, md5).
// Runs without networking or DB, so it works on every platform.

#include "utils/string_helper.hpp"
#include "utils/number_helper.hpp"
#include "net/md5.hpp"
#include <cassert>
#include <cstdio>
#include <string>

#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL: %s @ %d\n", #cond, __LINE__); \
        std::exit(1); \
    } \
} while (0)

int main() {
    using namespace aniswitch;

    // ---- string_helper ----
    EXPECT(trim("  hello  ") == "hello");
    EXPECT(trim("") == ""    );
    EXPECT(split("a,b,c", ',').size() == 3);
    EXPECT(split("a,b,c", ',').back() == "c");
    EXPECT(icontains("Hello World", "world"));
    EXPECT(!icontains("Hello World", "xyz"));
    EXPECT(format_duration(65)   == "1:05");
    EXPECT(format_duration(3661) == "1:01:01");
    EXPECT(format_bytes(2048)    == "2.00 KB");

    // ---- number_helper ----
    EXPECT(format_thousands(1234567) == "1,234,567");
    EXPECT(format_compact(1500)      == "1.5k");
    EXPECT(format_compact(1500000)   == "1.5m");
    EXPECT(format_score(8.0)         == "8");
    EXPECT(format_score(8.7)         == "8.7");

    // ---- md5 ----
    // Standard test vectors from RFC 1321.
    EXPECT(md5("")          == "d41d8cd98f00b204e9800998ecf8427e");
    EXPECT(md5("a")         == "0cc175b9c0f1b6a831c399e269772661");
    EXPECT(md5("abc")       == "900150983cd24fb0d6963f7d28e17f72");
    EXPECT(md5("message digest") == "f96b697d7cb7938d525a2f31aaf161d0");
    EXPECT(md5("abcdefghijklmnopqrstuvwxyz")
           == "c3fcd3d76192e4007dfb496cca67e13b");

    std::puts("test_misc_utils: OK");
    return 0;
}
