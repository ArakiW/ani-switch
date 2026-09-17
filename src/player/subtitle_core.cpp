// SPDX-License-Identifier: AGPL-3.0
#include "player/subtitle_core.hpp"
#include "player/mpv_core.hpp"

namespace aniswitch {
std::vector<SubtitleTrack> SubtitleCore::tracks() {
    mpv_node node{};
    std::vector<SubtitleTrack> result;
    if (mpv_get_property(MPVCore::instance().getHandle(), "track-list", MPV_FORMAT_NODE, &node) < 0)
        return result;
    try {
        if (node.format == MPV_FORMAT_NODE_ARRAY) {
            for (int i = 0; i < node.u.list->num; ++i) {
                auto& item = node.u.list->values[i];
                if (item.format != MPV_FORMAT_NODE_MAP) continue;
                SubtitleTrack track;
                bool subtitle = false;
                for (int k = 0; k < item.u.list->num; ++k) {
                    std::string key = item.u.list->keys[k];
                    auto& value = item.u.list->values[k];
                    if (key == "type" && value.format == MPV_FORMAT_STRING) subtitle = std::string(value.u.string) == "sub";
                    if (key == "id" && value.format == MPV_FORMAT_INT64) track.id = value.u.int64;
                    if (key == "title" && value.format == MPV_FORMAT_STRING) track.title = value.u.string;
                    if (key == "lang" && value.format == MPV_FORMAT_STRING) track.language = value.u.string;
                    if (key == "selected" && value.format == MPV_FORMAT_FLAG) track.selected = value.u.flag;
                }
                if (subtitle) result.push_back(std::move(track));
            }
        }
    } catch (...) { mpv_free_node_contents(&node); throw; }
    mpv_free_node_contents(&node);
    return result;
}
void SubtitleCore::select(int64_t id) { MPVCore::instance().command_async("set", "sid", id); }
void SubtitleCore::disable() { MPVCore::instance().command_async("set", "sid", "no"); }
void SubtitleCore::add(const std::string& path) { MPVCore::instance().command_async("sub-add", path, "select"); }
}
