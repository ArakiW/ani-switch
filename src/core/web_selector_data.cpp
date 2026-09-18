// SPDX-License-Identifier: AGPL-3.0
#include "core/web_selector_data.hpp"
#include <nlohmann/json.hpp>

namespace aniswitch {

bool WebSelectorData::parse(const std::string& json, WebSelectorData& out) {
    out.sources.clear();
    out.rawJson = json;
    try {
        auto j = nlohmann::json::parse(json);
        // Path: exportedMediaSourceDataList.mediaSources[]
        const auto& list = j.at("exportedMediaSourceDataList").at("mediaSources");
        for (const auto& entry : list) {
            if (entry.value("factoryId", "") != "web-selector") continue;
            const auto& args = entry.at("arguments");
            WebSelectorSource s;
            s.name  = args.value("name", std::string{});
            s.iconUrl = args.value("iconUrl", std::string{});
            s.tier  = args.value("tier", 99);
            const auto& sc = args.at("searchConfig");
            s.searchUrl       = sc.value("searchUrl", std::string{});
            s.requestIntervalMs = sc.value("requestInterval", 3000);
            if (sc.contains("selectorSubjectFormatA"))
                s.selectorSubjectA = sc["selectorSubjectFormatA"].value("selectLists", std::string{});
            if (sc.contains("selectorChannelFormatFlattened")) {
                const auto& cf = sc["selectorChannelFormatFlattened"];
                s.selectorChannelFlattened = cf.value("selectEpisodeLists", std::string{});
                s.selectorEpisodesFromList = cf.value("selectEpisodesFromList", std::string{});
            }
            if (sc.contains("matchVideo")) {
                const auto& mv = sc["matchVideo"];
                try {
                    const std::string re = mv.value("matchVideoUrl", std::string{});
                    if (!re.empty()) {
                        s.matchVideoUrl = std::regex(re, std::regex::ECMAScript | std::regex::optimize);
                        s.matchVideoUrlPattern = re;
                        s.hasMatchVideoUrl = true;
                    }
                } catch (const std::regex_error&) {
                    // Bad regex from upstream — keep the source and fall
                    // back to the generic playerJsonUrl extractor.
                    s.hasMatchVideoUrl = false;
                }
                if (mv.contains("addHeadersToVideo")) {
                    s.referer   = mv["addHeadersToVideo"].value("referer",   std::string{});
                    s.userAgent = mv["addHeadersToVideo"].value("userAgent", std::string{});
                }
                s.cookies = mv.value("cookies", std::string{});
            }
            if (!s.searchUrl.empty() && !s.selectorSubjectA.empty()) {
                out.sources.push_back(std::move(s));
            }
        }
    } catch (const std::exception&) {
        out.sources.clear();
        return false;
    }
    return true;
}

}  // namespace aniswitch
