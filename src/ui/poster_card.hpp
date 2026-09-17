// SPDX-License-Identifier: AGPL-3.0
//
// v22: poster card for horizontal rails / grids.
// When theme::kCoverEnabled, downloads cover via ImageLoader
// (single worker + brls::sync). Color band is the fallback.
#pragma once

#include <borealis.hpp>
#include <borealis/core/thread.hpp>
#include <fmt/format.h>
#include <functional>
#include <string>
#include "net/bgm_types.hpp"
#include "ui/theme.hpp"
#include "utils/image_loader.hpp"

namespace aniswitch {

inline std::string pickImageUrl(const SubjectImage& img) {
    if (!img.large.empty()) return img.large;
    if (!img.common.empty()) return img.common;
    if (!img.medium.empty()) return img.medium;
    return {};
}

// Focusable poster cell. Cover sits above a 72px title plate.
inline brls::Box* makePosterCard(int32_t subjectId, const std::string& title,
                                 float score,
                                 const std::string& imageUrl = "",
                                 int w = theme::kPosterW,
                                 int h = theme::kPosterH,
                                 std::function<void()> onClick = nullptr) {
    auto* card = new brls::Box();
    card->setAxis(brls::Axis::COLUMN);
    card->setWidth(w);
    card->setHeight(h);
    card->setMarginRight(16);
    card->setMarginBottom(8);
    card->setCornerRadius(8);
    card->setBackground(brls::ViewBackground::SHAPE_COLOR);
    const int band = subjectId > 0 ? (subjectId % 5) : 0;
    static const NVGcolor kBands[5] = {
        nvgRGB(75, 55, 120), nvgRGB(45, 70, 110), nvgRGB(90, 50, 70),
        nvgRGB(45, 95, 85),  nvgRGB(70, 60, 115),
    };
    card->setBackgroundColor(kBands[band]);
    card->setFocusable(true);
    theme::applyFocusStyle(card, 10.0f, 8.0f);

    brls::Image* cover = nullptr;
    if (theme::kCoverEnabled && !imageUrl.empty()) {
        cover = new brls::Image();
        cover->setWidth(w);
        cover->setHeight(h - 72);
        cover->setScalingType(brls::ImageScalingType::FILL);
        card->addView(cover);
    } else {
        auto* spacer = new brls::Box();
        spacer->setGrow(1.0f);
        card->addView(spacer);
    }

    auto* plate = new brls::Box();
    plate->setAxis(brls::Axis::COLUMN);
    plate->setHeight(72);
    plate->setPadding(10, 10, 8, 10);
    plate->setBackground(brls::ViewBackground::SHAPE_COLOR);
    plate->setBackgroundColor(nvgRGBA(11, 9, 16, 200));
    auto* t = new brls::Label();
    t->setText(title.empty() ? "—" : title);
    t->setFontSize(theme::kTypeCard);
    t->setTextColor(theme::kDarkTextPrimary);
    t->setSingleLine(true);
    plate->addView(t);
    if (score > 0) {
        auto* s = new brls::Label();
        s->setText(fmt::format("★ {:.1f}", score));
        s->setFontSize(theme::kTypeMicro);
        s->setTextColor(nvgRGB(255, 200, 80));
        plate->addView(s);
    }
    card->addView(plate);

    if (onClick) {
        card->registerClickAction([onClick](brls::View*) {
            onClick();
            return true;
        });
    }

    if (cover) {
        ImageLoader::instance().load(
            imageUrl,
            [cover](const std::string& path) {
                if (path.empty()) return;
                brls::sync([cover, path]() { cover->setImageFromFile(path); });
            },
            fmt::format("poster-{}", subjectId));
    }
    return card;
}

inline brls::Box* makePosterCardFromSearch(const SearchSubject& s,
                                           std::function<void()> onClick = nullptr) {
    const std::string t = !s.nameCN.empty() ? s.nameCN : s.name;
    return makePosterCard(s.id, t.empty() ? ("subject " + std::to_string(s.id)) : t,
                          static_cast<float>(s.score), pickImageUrl(s.images),
                          theme::kPosterW, theme::kPosterH, std::move(onClick));
}

}  // namespace aniswitch
