// SPDX-License-Identifier: GPL-3.0-or-later
#include "player/tsvitch_video_view.hpp"
#include "player/tsvitch_video_progress_slider.hpp"
#include "player/tsvitch_svg_image.hpp"
#include "player/tsvitch_video_profile.hpp"
#include "player/mpv_core.hpp"
#include "utils/config_helper.hpp"
#include <borealis/views/label.hpp>
#include <borealis/views/progress_spinner.hpp>
#include <borealis/core/logger.hpp>
#include <fmt/format.h>
#include <cstdio>
#include <ctime>

#if defined(__SWITCH__)
extern "C" void aniswitchStartupLog(const char*);
#define VLOG(m) aniswitchStartupLog(m)
#else
#define VLOG(m) do { (void)0; } while (0)
#endif

using namespace brls::literals;

VideoView::VideoView() {
    mpvCore_ = &aniswitch::MPVCore::instance();
    VLOG("VideoView: ctor begin");
    try {
        this->inflateFromXMLRes("xml/views/video_view.xml");
        VLOG("VideoView: ctor done");
    } catch (const std::exception& e) {
        brls::Logger::error("VideoView: XML inflate failed: {}", e.what());
        VLOG("VideoView: XML inflate failed");
    }
    brls::Logger::info("VideoView (TsVitch graft): create");
    if (osdSlider) {
        // wiliwili: seek by real_duration when set, else percent.
        osdSlider->getProgressSetEvent()->subscribe([this](float progress) {
            showOSD(true);
            if (real_duration_ > 0) {
                mpvCore_->seek(static_cast<int64_t>(
                    static_cast<float>(real_duration_) * progress));
            } else {
                mpvCore_->seekPercent(progress);
            }
        });
    }
    if (btnToggle) {
        btnToggle->registerClickAction([this](brls::View*) {
            togglePlay();
            return true;
        });
    }
}

VideoView::~VideoView() {
    VLOG("VideoView: dtor begin");
    unRegisterMpvEvent();
    VLOG("VideoView: dtor done");
}

void VideoView::setUrl(const std::string& url) {
    showLoading();
    hideCenterHint();
    {
        char b[200];
        snprintf(b, sizeof(b), "VideoView: setUrl len=%zu net=%d allowNet=%d",
                 url.size(),
                 (url.rfind("http", 0) == 0) ? 1 : 0,
                 aniswitch::MPVCore::ALLOW_NETWORK_URL ? 1 : 0);
        VLOG(b);
    }
    // Animeko-source HLS mirrors require a Referer; set before loadfile.
    if (url.find("m3u8") != std::string::npos ||
        url.find("rrcdnbf") != std::string::npos ||
        url.find("bfengbf") != std::string::npos) {
        mpvCore_->command_async("set", "http-header-fields",
                                "Referer: https://www.akianime.cc/");
        mpvCore_->command_async("set", "user-agent",
                                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                                "AppleWebKit/537.36 (KHTML, like Gecko) "
                                "Chrome/120.0.0.0 Safari/537.36");
    }
    // wiliwili-style extra for network loads: referrer + timeout + proxy.
    if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) {
        std::string extra =
            "referrer=\"https://www.akianime.cc/\",network-timeout=8";
        const auto proxy = aniswitch::ProgramConfig::instance().getProxy();
        if (!proxy.empty()) {
            extra += ",http-proxy=\"" + proxy + "\"";
        }
        mpvCore_->setUrl(url, extra);
    } else {
        mpvCore_->setUrl(url);
    }
    registerMpvEvent();
    brls::Logger::info("VideoView: setUrl {}", url);
}

void VideoView::resume() { mpvCore_->resume(); }
void VideoView::pause() { mpvCore_->pause(); }
void VideoView::stop() { mpvCore_->stop(); }

void VideoView::togglePlay() {
    if (mpvCore_->isStopped()) return;
    if (mpvCore_->isPaused()) mpvCore_->resume();
    else mpvCore_->pause();
}

void VideoView::showOSD(bool temp) {
    is_osd_shown_ = true;
    if (osdTopBox) osdTopBox->setVisibility(brls::Visibility::VISIBLE);
    if (osdBottomBox) osdBottomBox->setVisibility(brls::Visibility::VISIBLE);
    if (temp) osdLastShowTime_ = std::time(nullptr);
}

void VideoView::hideOSD() {
    is_osd_shown_ = false;
    if (osdTopBox) osdTopBox->setVisibility(brls::Visibility::GONE);
    if (osdBottomBox) osdBottomBox->setVisibility(brls::Visibility::GONE);
}

bool VideoView::isOSDShown() const { return is_osd_shown_; }
bool VideoView::isOSDLock() const { return is_osd_lock_; }

void VideoView::toggleOSD() {
    if (is_osd_shown_) hideOSD();
    else showOSD(true);
}

void VideoView::toggleOSDLock() {
    is_osd_lock_ = !is_osd_lock_;
    if (osdLockIcon) {
        osdLockIcon->setImageFromSVGRes(is_osd_lock_ ? "svg/player-lock.svg"
                                                     : "svg/player-unlock.svg");
    }
}

void VideoView::showLoading() {
    if (osdSpinner) osdSpinner->setVisibility(brls::Visibility::VISIBLE);
}

void VideoView::hideLoading() {
    if (osdSpinner) osdSpinner->setVisibility(brls::Visibility::GONE);
}

void VideoView::showCenterHint() {
    if (osdCenterBox2) osdCenterBox2->setVisibility(brls::Visibility::VISIBLE);
}

void VideoView::hideCenterHint() {
    if (osdCenterBox2) osdCenterBox2->setVisibility(brls::Visibility::GONE);
}

void VideoView::setCenterHintText(const std::string& text) {
    if (centerLabel2) centerLabel2->setText(text);
    showCenterHint();
    hideLoading();
}

void VideoView::showHint(const std::string& value) {
    if (hintLabel) hintLabel->setText(value);
    if (hintBox) hintBox->setVisibility(brls::Visibility::VISIBLE);
}

void VideoView::clearHint() {
    if (hintBox) hintBox->setVisibility(brls::Visibility::GONE);
}

void VideoView::setTitle(const std::string& title) {
    if (videoTitleLabel) videoTitleLabel->setText(title);
}

std::string VideoView::getTitle() {
    return videoTitleLabel ? videoTitleLabel->getFullText() : "";
}

void VideoView::setDuration(const std::string& value) {
    if (rightStatusLabel) rightStatusLabel->setText(value);
}

void VideoView::setPlaybackTime(const std::string& value) {
    if (leftStatusLabel) leftStatusLabel->setText(value);
}

void VideoView::setProgress(float value) {
    if (osdSlider) osdSlider->setProgress(value);
}

float VideoView::getProgress() {
    return osdSlider ? osdSlider->getProgress() : 0.f;
}

void VideoView::setRealDuration(int seconds) {
    real_duration_ = seconds > 0 ? seconds : 0;
    if (real_duration_ > 0) {
        const int s = real_duration_;
        setDuration(fmt::format("{:d}:{:02d}", s / 60, s % 60));
        if (mpvCore_) {
            const double p = static_cast<double>(mpvCore_->video_progress);
            setProgress(static_cast<float>(p / static_cast<double>(real_duration_)));
        }
    }
}

int VideoView::getRealDuration() const { return real_duration_; }

void VideoView::setVideoMode() {}
void VideoView::setLiveMode() {}
void VideoView::setAdMode() {}

void VideoView::setStatusLabelLeft(const std::string& value) {
    if (leftStatusLabel) leftStatusLabel->setText(value);
}
void VideoView::setStatusLabelRight(const std::string& value) {
    if (rightStatusLabel) rightStatusLabel->setText(value);
}
void VideoView::disableCloseOnEndOfFile() { closeOnEndOfFile_ = false; }
void VideoView::showVideoProgressSlider() {
    if (osdSlider) osdSlider->setVisibility(brls::Visibility::VISIBLE);
}
void VideoView::hideVideoProgressSlider() {
    if (osdSlider) osdSlider->setVisibility(brls::Visibility::GONE);
}
void VideoView::disableProgressSliderSeek(bool) {}
void VideoView::hideStatusLabel() {
    if (leftStatusLabel) leftStatusLabel->setVisibility(brls::Visibility::GONE);
    if (rightStatusLabel) rightStatusLabel->setVisibility(brls::Visibility::GONE);
}
void VideoView::hideOSDLockButton() {
    if (osdLockBox) osdLockBox->setVisibility(brls::Visibility::GONE);
}
void VideoView::setFullscreenIcon(bool) {}
void VideoView::setFavoriteIcon(bool fav) { isFavorite_ = fav; }

void VideoView::toggleFavorite() {
    isFavorite_ = !isFavorite_;
    if (favoriteCb_) favoriteCb_(isFavorite_);
}

void VideoView::registerMpvEvent() {
    if (registerMPVEvent_) return;
    registerMPVEvent_ = true;
    eventSubscribeID_ = mpvCore_->getEvent()->subscribe([this](MpvEventEnum event) {
        // Force a redraw so mpv frames actually hit the screen.
        this->invalidate();
        switch (event) {
            case MpvEventEnum::LOADING_START:
                VLOG("player: loading start");
                showLoading();
                break;
            case MpvEventEnum::LOADING_END:
                VLOG("player: loading end");
                hideLoading();
                hideCenterHint();
                this->invalidate();
                break;
            case MpvEventEnum::MPV_PAUSE:
                VLOG("player: pause");
                hideLoading();
                hideCenterHint();
                this->invalidate();
                break;
            case MpvEventEnum::MPV_RESUME:
                VLOG("player: play");
                hideLoading();
                hideCenterHint();
                this->invalidate();
                break;
            case MpvEventEnum::MPV_LOADED:
                VLOG("player: mpv loaded");
                hideLoading();
                hideCenterHint();
                this->invalidate();
                break;
            case MpvEventEnum::START_FILE:
                VLOG("player: start file");
                hideLoading();
                hideCenterHint();
                this->invalidate();
                break;
            case MpvEventEnum::UPDATE_DURATION: {
                double d = 0;
                if (real_duration_ > 0) {
                    d = static_cast<double>(real_duration_);
                } else {
                    d = mpvCore_->getDouble("duration");
                    if (d <= 0) d = static_cast<double>(mpvCore_->duration);
                    if (d <= 0) d = static_cast<double>(mpvCore_->video_progress);
                }
                int s = static_cast<int>(d);
                setDuration(fmt::format("{:d}:{:02d}", s / 60, s % 60));
                break;
            }
            case MpvEventEnum::UPDATE_PROGRESS: {
                double p = static_cast<double>(mpvCore_->video_progress);
                double d = 0;
                if (real_duration_ > 0) {
                    d = static_cast<double>(real_duration_);
                } else {
                    d = mpvCore_->getDouble("duration");
                    if (d <= 0) d = static_cast<double>(mpvCore_->duration);
                }
                if (d > 0) setProgress(static_cast<float>(p / d));
                int s = static_cast<int>(p);
                setPlaybackTime(fmt::format("{:d}:{:02d}", s / 60, s % 60));
                // Right label always shows full real duration when known.
                if (real_duration_ > 0) {
                    const int rd = real_duration_;
                    setDuration(fmt::format("{:d}:{:02d}", rd / 60, rd % 60));
                }
                break;
            }
            case MpvEventEnum::END_OF_FILE:
                VLOG("player: end of file");
                if (onEndCb_) onEndCb_();
                break;
            case MpvEventEnum::MPV_FILE_ERROR:
                VLOG("player: file error (UI)");
                hideLoading();
                setCenterHintText("播放失败");
                break;
            default:
                break;
        }
    });
}

void VideoView::unRegisterMpvEvent() {
    if (!registerMPVEvent_) return;
    mpvCore_->getEvent()->unsubscribe(eventSubscribeID_);
    registerMPVEvent_ = false;
}

brls::View* VideoView::create() { return new VideoView(); }

void VideoView::draw(NVGcontext* vg, float x, float y, float width, float height,
                     brls::Style style, brls::FrameContext* ctx) {
    // Paint mpv FIRST every frame (same as aniswitch::VideoView).
    if (mpvCore_ && mpvCore_->isValid() && width > 0 && height > 0) {
        mpvCore_->draw(brls::Rect(x, y, width, height), 1.0f);
        if (!loggedFirstFrame_) {
            loggedFirstFrame_ = true;
            VLOG("player: first frame drawn");
        }
    }
    // OSD chrome on top of the video.
    Box::draw(vg, x, y, width, height, style, ctx);
    if (osdSlider) {
        float p = osdSlider->getProgress();
        if (p < 0) p = 0;
        if (p > 1) p = 1;
        nvgBeginPath(vg);
        nvgFillColor(vg, nvgRGBAf(0.65f, 0.54f, 0.98f, 0.9f));
        nvgRect(vg, x, y + height - 3, width * p, 3);
        nvgFill(vg);
    }
    if (is_osd_shown_ && !is_osd_lock_) {
        std::time_t now = std::time(nullptr);
        if (osdLastShowTime_ > 0 && now - osdLastShowTime_ > 5) hideOSD();
    }
}

void VideoView::onLayout() {
    brls::View::onLayout();
    if (mpvCore_) {
        brls::Rect f = getFrame();
        if (f.getWidth() > 1 && f.getHeight() > 1)
            mpvCore_->setFrameSize(f);
    }
}

brls::View* VideoView::getDefaultFocus() {
    return btnToggle ? static_cast<brls::View*>(btnToggle)
                     : Box::getDefaultFocus();
}
