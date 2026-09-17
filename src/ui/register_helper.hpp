// SPDX-License-Identifier: AGPL-3.0
//
// ani-switch PATCH: register TsVitch player XML views for borealis.
#pragma once

#include <borealis.hpp>
#include "player/tsvitch_video_view.hpp"
#include "player/tsvitch_svg_image.hpp"
#include "player/tsvitch_video_profile.hpp"
#include "player/tsvitch_video_progress_slider.hpp"
#include "player/tsvitch_hint_label.hpp"

namespace aniswitch {

inline void registerViewsForXML() {
    brls::Application::registerXMLView("VideoView", VideoView::create);
    brls::Application::registerXMLView("SVGImage", SVGImage::create);
    brls::Application::registerXMLView("VideoProfile", VideoProfile::create);
    brls::Application::registerXMLView("VideoProgressSlider", VideoProgressSlider::create);
    brls::Application::registerXMLView("HintLabel", HintLabel::create);
}

inline void registerAllViews() { registerViewsForXML(); }

}  // namespace aniswitch
