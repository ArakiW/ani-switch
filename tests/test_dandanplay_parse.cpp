// SPDX-License-Identifier: AGPL-3.0
//
// Smoke test for the dandanplay danmaku XML parser + JSON struct
// round-trip. We deliberately avoid including dandanplay_client.hpp
// because that pulls cpr which needs CMake-generated headers.

#include "net/dandan_parser.hpp"   // forwards to danmaku_parser (header-only)
#include <cassert>
#include <cstdio>
#include <nlohmann/json.hpp>

#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL: %s @ %d\n", #cond, __LINE__); \
        std::exit(1); \
    } \
} while (0)

int main() {
    using namespace aniswitch;

    // ---- XML path ----
    const std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<i>
  <d p="1.234,1,25,16777215,1700000000,abc">hello xml</d>
  <d p="5.678,5,36,16711680,1700000000,def">top xml</d>
  <d p="7.000,7,40,16776960,1700000000,xyz">advanced xml</d>
</i>
)";
    auto xparsed = parseDandanXml(xml);
    EXPECT(xparsed.size() == 3);
    EXPECT(xparsed[0].text == "hello xml");
    EXPECT(xparsed[0].time == 1234);
    EXPECT(xparsed[0].mode == 1);
    EXPECT(xparsed[2].mode == 5);

    // ---- JSON round-trip ----
    auto j = nlohmann::json::parse(R"([
        { "time": 1234, "mode": 1, "fontSize": 25, "color": 16777215,
          "text": "hello", "author": "tester" },
        { "time": 5678, "mode": 5, "fontSize": 36, "color": 16711680,
          "text": "top", "author": "tester2" }
    ])");
    EXPECT(j.is_array());
    EXPECT(j.size() == 2);
    EXPECT(j[0]["text"] == "hello");
    EXPECT(j[1]["mode"] == 5);

    auto entities = parseDandanXml("<i><d p='0,1,25,16777215'>A &amp; B &lt;3</d></i>");
    EXPECT(entities.size() == 1 && entities[0].text == "A & B <3");
    EXPECT(parseDandanXml("<i><d p='nan,1,25,1'>bad</d></i>").empty());
    EXPECT(parseDandanXml("<i><d p='1e300,1,25,1'>bad</d></i>").empty());
    EXPECT(parseDandanXml("<i><d p='1,1,1e50,1'>bad</d></i>").empty());
    EXPECT(parseDandanXml("<i><d p='1,1,25,1'>unclosed").empty());
    DandanComment comment;
    comment.time = 1250;
    comment.mode = 7;
    comment.text = "test";
    auto jsonParsed = parseDandanJson({comment});
    EXPECT(jsonParsed.size() == 1 && jsonParsed[0].mode == 5 && jsonParsed[0].time == 1250);

    std::puts("test_dandanplay_parse: OK");
    return 0;
}
