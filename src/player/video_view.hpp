// SPDX-License-Identifier: AGPL-3.0
#pragma once
#include <borealis/core/view.hpp>

namespace aniswitch {
class VideoView : public brls::View {
public:
    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;
};
}
