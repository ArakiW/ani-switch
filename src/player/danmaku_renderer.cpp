// SPDX-License-Identifier: AGPL-3.0
#include "player/danmaku_renderer.hpp"
#include "player/mpv_core.hpp"
#include <fmt/format.h>
#include <algorithm>

namespace aniswitch {

DanmakuRenderer& DanmakuRenderer::instance() {
    static DanmakuRenderer renderer;
    return renderer;
}

DanmakuItem DanmakuRenderer::toItem(const ParsedDanmaku& p) {
    auto attrs = fmt::format("{:.3f},{},{},{},0,0,0,0,10",
                             p.time / 1000.0, p.mode, p.fontSize, p.color);
    return DanmakuItem(p.text, attrs.c_str());
}

void DanmakuRenderer::loadHistorical(const std::vector<ParsedDanmaku>& items) {
    std::vector<DanmakuItem> data;
    data.reserve(items.size());
    // Drop exact-time duplicates; the first source that reports a
    // timestamp wins.  (dandanplay and myani usually don't overlap
    // for the same episode but it can happen for live + cached.)
    int64_t lastTime = -1;
    for (const auto& item : items) {
        if (static_cast<int64_t>(item.time) == lastTime) continue;
        lastTime = item.time;
        data.push_back(toItem(item));
    }
    std::stable_sort(data.begin(), data.end());
    DanmakuCore::instance().loadDanmakuData(data);
}

void DanmakuRenderer::sendLocal(const std::string& text, int32_t mode,
                                int32_t color, int32_t fontSize) {
    if (text.empty()) return;
    ParsedDanmaku p;
    p.time     = static_cast<int64_t>(MPVCore::instance().getPlaybackTime() * 1000);
    p.mode     = mode;
    p.color    = color;
    p.fontSize = fontSize;
    p.text     = text;
    DanmakuCore::instance().addSingleDanmaku(toItem(p));
}

void DanmakuRenderer::clear() {
    DanmakuCore::instance().reset();
}

}  // namespace aniswitch
