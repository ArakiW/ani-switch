// SPDX-License-Identifier: AGPL-3.0
//
// Unit tests for the myani danmaku protocol data types.  We avoid
// including net/myani_client.hpp (which pulls in cpr) and instead go
// through net/myani_types.hpp which is just plain C++ types.  This
// keeps the test fast and linkable everywhere.

#include "net/myani_types.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

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
    using namespace aniswitch;

    // ---- Location enum round-trip ----
    // toParsed() converts the wire enum to an integer mode the
    // DanmakuCore layer understands.
    MyaniDanmaku m;
    m.location = MyaniLocation::TOP;
    EXPECT_EQ(toParsed(m).mode, 1);
    m.location = MyaniLocation::BOTTOM;
    EXPECT_EQ(toParsed(m).mode, 4);
    m.location = MyaniLocation::NORMAL;
    EXPECT_EQ(toParsed(m).mode, 2);
    m.location = MyaniLocation::ADV;
    EXPECT_EQ(toParsed(m).mode, 3);

    // ---- String mapping ----
    // myaniLocationToString must round-trip the well-known names so
    // POST /v1/danmaku bodies look exactly like the upstream client.
    EXPECT(std::strcmp(myaniLocationToString(MyaniLocation::TOP),    "TOP")    == 0);
    EXPECT(std::strcmp(myaniLocationToString(MyaniLocation::BOTTOM), "BOTTOM") == 0);
    EXPECT(std::strcmp(myaniLocationToString(MyaniLocation::NORMAL), "NORMAL") == 0);
    EXPECT(std::strcmp(myaniLocationToString(MyaniLocation::ADV),    "ADV")    == 0);

    // ---- Defaults ----
    // A default-constructed MyaniDanmaku must have sane defaults so
    // the parser can skip missing fields without crashing.
    EXPECT_EQ(m.id, 0);
    EXPECT_EQ(m.playTimeMs, 0);
    EXPECT_EQ(m.color, 0xFFFFFF);
    EXPECT(m.text.empty());
    EXPECT(m.senderId.empty());

    // ---- toParsed() field mapping ----
    MyaniDanmaku m2;
    m2.id = 99;
    m2.playTimeMs = 5000;
    m2.color = 0xFF0000;
    m2.location = MyaniLocation::NORMAL;
    m2.text = "hello";
    m2.senderId = "user-42";
    auto p = toParsed(m2);
    EXPECT_EQ(p.time, 5000);
    EXPECT_EQ(p.mode, 2);
    EXPECT_EQ(p.color, 0xFF0000);
    EXPECT_EQ(p.text, std::string("hello"));

    std::puts("test_myani_parse: OK");
    return 0;
}
