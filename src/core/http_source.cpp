// SPDX-License-Identifier: AGPL-3.0
#include "core/http_source.hpp"
#include "utils/config_helper.hpp"
#include <fstream>

namespace aniswitch {
void HTTPSourceProvider::enumerate(int32_t episodeId, SourceListCb callback,
                                   std::function<void(const std::string&, int)> error) {
    std::vector<VideoSource> sources;
    try {
        std::ifstream file(ProgramConfig::instance().getConfigDir() + "/sources.json");
        if (file) {
            nlohmann::json manifest;
            file >> manifest;
            const auto key = std::to_string(episodeId);
            if (manifest.contains(key)) {
                for (const auto& entry : manifest.at(key)) {
                    VideoSource source;
                    source.url = entry.at("url").get<std::string>();
                    source.label = entry.value("label", "HTTP");
                    source.priority = entry.value("priority", 0);
                    if (source.url.rfind("http://", 0) != 0 && source.url.rfind("https://", 0) != 0)
                        throw std::runtime_error("sources.json requires HTTP(S) video URLs");
                    sources.push_back(std::move(source));
                }
            }
        }
    } catch (const std::exception& e) {
        if (error) error(e.what(), -1);
        return;
    }
    if (callback) callback(std::move(sources));
}
}
