// SPDX-License-Identifier: AGPL-3.0
#include "ui/status_view.hpp"
#include "ui/theme.hpp"

namespace aniswitch {

namespace {
char stateGlyph(StatusView::State s) {
    switch (s) {
        case StatusView::State::Loading: return '.';
        case StatusView::State::Empty:   return 'o';
        case StatusView::State::Error:   return '!';
    }
    return '?';
}
NVGcolor stateColor(StatusView::State s) {
    switch (s) {
        case StatusView::State::Loading: return nvgRGB(180, 200, 230);
        case StatusView::State::Empty:   return nvgRGB(160, 160, 160);
        case StatusView::State::Error:   return nvgRGB(220, 110, 110);
    }
    return nvgRGB(255, 255, 255);
}
}  // namespace

StatusView::StatusView() {
    setAxis(brls::Axis::COLUMN);
    setAlignItems(brls::AlignItems::CENTER);
    setJustifyContent(brls::JustifyContent::CENTER);
    setPadding(40);

    // Top: spinner (Loading) OR a big glyph (Empty/Error).
    iconBox_ = new brls::Box();
    iconBox_->setAxis(brls::Axis::COLUMN);
    iconBox_->setAlignItems(brls::AlignItems::CENTER);
    iconBox_->setJustifyContent(brls::JustifyContent::CENTER);
    iconBox_->setPadding(20, 0, 0, 0);
    addView(iconBox_);

    spinner_ = new brls::ProgressSpinner(brls::ProgressSpinnerSize::LARGE);
    spinner_->setVisibility(brls::Visibility::VISIBLE);
    iconBox_->addView(spinner_);

    // Big glyph for Empty/Error.
    title_ = new brls::Label();
    title_->setFontSize(56);
    title_->setSingleLine(true);
    title_->setVisibility(brls::Visibility::GONE);
    iconBox_->addView(title_);

    // Detail line.
    detail_ = new brls::Label();
    detail_->setFontSize(theme::kTypeCaption);
    detail_->setTextColor(nvgRGB(180, 180, 180));
    detail_->setSingleLine(false);
    detail_->setMarginTop(12);
    addView(detail_);

    // Retry button (only visible in Error state with onRetry).
    retry_ = new brls::Button();
    retry_->setText("重试");
    retry_->setMarginTop(16);
    retry_->setVisibility(brls::Visibility::GONE);
    retry_->registerClickAction([this](brls::View*) {
        if (onRetry_) onRetry_();
        return true;
    });
    addView(retry_);
}

void StatusView::setLoading(const std::string& message) {
    state_ = State::Loading;
    spinner_->setVisibility(brls::Visibility::VISIBLE);
    title_->setVisibility(brls::Visibility::GONE);
    detail_->setText(message.empty() ? std::string("加载中...") : message);
    retry_->setVisibility(brls::Visibility::GONE);
}

void StatusView::setEmpty(const std::string& message) {
    state_ = State::Empty;
    spinner_->setVisibility(brls::Visibility::GONE);
    title_->setText(std::string(1, stateGlyph(State::Empty)));
    title_->setTextColor(stateColor(State::Empty));
    title_->setVisibility(brls::Visibility::VISIBLE);
    detail_->setText(message.empty() ? std::string("暂无数据") : message);
    retry_->setVisibility(brls::Visibility::GONE);
}

void StatusView::setError(const std::string& message,
                           std::function<void()> onRetry) {
    state_ = State::Error;
    spinner_->setVisibility(brls::Visibility::GONE);
    title_->setText(std::string(1, stateGlyph(State::Error)));
    title_->setTextColor(stateColor(State::Error));
    title_->setVisibility(brls::Visibility::VISIBLE);
    detail_->setText(message);
    onRetry_ = std::move(onRetry);
    retry_->setVisibility(onRetry_ ? brls::Visibility::VISIBLE
                                    : brls::Visibility::GONE);
}

}  // namespace aniswitch
