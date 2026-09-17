// SPDX-License-Identifier: AGPL-3.0
#include "net/danmaku_parser.hpp"
#include <borealis/extern/tinyxml2/tinyxml2.h>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace aniswitch {

std::vector<ParsedDanmaku> parseDandanJson(const std::vector<DandanComment>& in) {
    std::vector<ParsedDanmaku> out;
    out.reserve(in.size());
    for (const auto& c : in) {
        ParsedDanmaku p;
        p.time = c.time;
        p.mode = c.mode == 7 ? 5 : c.mode;
        p.fontSize = c.fontSize;
        p.color = c.color;
        p.text = c.text;
        out.push_back(std::move(p));
    }
    return out;
}

std::vector<ParsedDanmaku> parseDandanXml(const std::string& xml) {
    tinyxml2::XMLDocument document;
    if (document.Parse(xml.data(), xml.size()) != tinyxml2::XML_SUCCESS) return {};
    auto* root = document.FirstChildElement("i");
    if (!root) return {};
    std::vector<ParsedDanmaku> out;
    for (auto* element = root->FirstChildElement("d"); element;
         element = element->NextSiblingElement("d")) {
        const char* cursor = element->Attribute("p");
        if (!cursor) continue;
        double fields[4]{};
        bool valid = true;
        for (int i = 0; i < 4; ++i) {
            char* end = nullptr;
            fields[i] = std::strtod(cursor, &end);
            if (end == cursor || !std::isfinite(fields[i]) || fields[i] < 0 ||
                (i < 3 ? *end != ',' : (*end != ',' && *end != '\0'))) {
                valid = false;
                break;
            }
            cursor = *end == ',' ? end + 1 : end;
        }
        double milliseconds = fields[0] * 1000.0;
        if (!valid || !std::isfinite(milliseconds) ||
            milliseconds >= static_cast<double>(std::numeric_limits<int64_t>::max())) continue;
        for (int i = 1; i < 4; ++i)
            if (fields[i] > std::numeric_limits<int32_t>::max()) valid = false;
        if (!valid) continue;
        ParsedDanmaku item;
        item.time = static_cast<int64_t>(milliseconds);
        item.mode = static_cast<int32_t>(fields[1]);
        item.fontSize = static_cast<int32_t>(fields[2]);
        item.color = static_cast<int32_t>(fields[3]);
        if (item.mode == 7) item.mode = 5;
        if (const char* text = element->GetText()) item.text = text;
        out.push_back(std::move(item));
    }
    return out;
}

}  // namespace aniswitch
