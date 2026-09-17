// SPDX-License-Identifier: AGPL-3.0
#include "player/video_view.hpp"
#include "player/mpv_core.hpp"
#include "player/danmaku_core.hpp"

namespace aniswitch {
void VideoView::draw(NVGcontext* vg, float x, float y, float width, float height,
                     brls::Style, brls::FrameContext*) {
    auto& player = MPVCore::instance();
    if (!player.isValid() || width <= 0 || height <= 0) return;
    player.draw(brls::Rect(x, y, width, height), getAlpha());
    nvgSave(vg);
    DanmakuCore::instance().draw(vg, x, y, width, height, getAlpha());
    nvgRestore(vg);
}
}
