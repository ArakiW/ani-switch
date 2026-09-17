// SPDX-License-Identifier: AGPL-3.0
#include "ui/fragment/subject_cell.hpp"
#include "ui/theme.hpp"
#include "utils/image_loader.hpp"
#include <fmt/format.h>
#include <borealis/core/thread.hpp>

namespace aniswitch {

SubjectCell::SubjectCell() {
    setAxis(brls::Axis::ROW);
    setPadding(12, 10, 12, 10);
    setMarginBottom(8);
    setBackground(brls::ViewBackground::SHAPE_COLOR);
    setBackgroundColor(theme::kDarkCardRowBg);
    setCornerRadius(8);
    // v22 P0: list row height for 10-foot readability.
    setHeight(theme::kRowHeight);
    setWidthPercentage(100.0f);
    setFocusable(true);
    theme::applyFocusStyle(this, 8.0f);
    setAlpha(theme::kUnfocusAlpha);

    // Cover (gated by theme::kCoverEnabled — same switch as SubjectActivity).
    cover_ = new brls::Image();
    cover_->setWidth(56);
    cover_->setHeight(84);
    cover_->setScalingType(brls::ImageScalingType::FILL);
    cover_->setCornerRadius(6);
    addView(cover_);

    auto* textBox = new brls::Box();
    textBox->setAxis(brls::Axis::COLUMN);
    textBox->setMarginLeft(12);
    textBox->setGrow(1.0f);
    textBox->setHeight(64);
    textBox->setJustifyContent(brls::JustifyContent::CENTER);
    addView(textBox);

    title_ = new brls::Label();
    title_->setFontSize(theme::kTypeH3);
    title_->setSingleLine(true);
    title_->setMarginBottom(4);
    textBox->addView(title_);

    subtitle_ = new brls::Label();
    subtitle_->setFontSize(theme::kTypeCaption);
    subtitle_->setTextColor(theme::kDarkTextSecondary);
    subtitle_->setSingleLine(true);
    subtitle_->setMarginBottom(2);
    textBox->addView(subtitle_);

    meta_ = new brls::Label();
    meta_->setFontSize(theme::kTypeCaption);
    meta_->setTextColor(theme::kAccentSoft);
    meta_->setSingleLine(true);
    textBox->addView(meta_);
}

void SubjectCell::setSubject(const Subject& s, const std::string& imageUrl) {
    subjectId_ = s.id;
    title_->setText(s.nameCN.empty() ? s.name : s.nameCN);
    std::string sub = s.nameCN.empty() ? "" : s.name;
    subtitle_->setText(sub);
    if (s.rating.score > 0) {
        meta_->setText(fmt::format("评分 {:.1f}", s.rating.score));
    } else {
        meta_->setText("");
    }
    // v22 P0: unified cover gate. Flip theme::kCoverEnabled only after
    // the single-thread ImageLoader stability experiment passes on device.
    if (!theme::kCoverEnabled || imageUrl.empty()) {
        return;
    }
    ImageLoader::instance().load(
        imageUrl,
        [this](const std::string& path) {
            if (path.empty()) return;
            brls::sync([this, path]() {
                if (cover_) cover_->setImageFromFile(path);
            });
        },
        fmt::format("subject-cover-{}", s.id));
}

void SubjectCell::setOnClick(std::function<void(int32_t)> onClick) {
    onClick_ = std::move(onClick);
    if (onClick_) {
        registerClickAction([this](brls::View*) {
            if (onClick_) onClick_(subjectId_);
            return true;
        });
    }
}

void SubjectCell::onFocusGained() {
    brls::Box::onFocusGained();
    setAlpha(1.0f);
}

void SubjectCell::onFocusLost() {
    brls::Box::onFocusLost();
    setAlpha(theme::kUnfocusAlpha);
}

}  // namespace aniswitch
