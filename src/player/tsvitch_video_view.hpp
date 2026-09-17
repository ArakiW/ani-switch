// SPDX-License-Identifier: GPL-3.0-or-later
//
// v22 graft: TsVitch VideoView OSD UI (XML layout) on ani-switch mpv_core.
// UI ids match resources/xml/views/video_view.xml from TsVitch.
#pragma once

#include <borealis/core/bind.hpp>
#include <borealis/core/box.hpp>
#include <borealis/core/application.hpp>
#include <functional>
#include <string>
#include "player/view/event_helper.hpp"

namespace brls {
class Label;
class ProgressSpinner;
}
class VideoProgressSlider;
class SVGImage;
class VideoProfile;
namespace aniswitch { class MPVCore; }

class VideoView : public brls::Box {
public:
    VideoView();
    ~VideoView() override;

    void setUrl(const std::string& url);
    void resume();
    void pause();
    void stop();
    void togglePlay();

    void showOSD(bool temp = true);
    void hideOSD();
    bool isOSDShown() const;
    bool isOSDLock() const;
    void toggleOSD();
    void toggleOSDLock();

    void showLoading();
    void hideLoading();
    void showCenterHint();
    void hideCenterHint();
    void setCenterHintText(const std::string& text);
    void showHint(const std::string& value);
    void clearHint();

    void setTitle(const std::string& title);
    std::string getTitle();
    void setDuration(const std::string& value);
    void setPlaybackTime(const std::string& value);
    void setProgress(float value);
    float getProgress();

    void setVideoMode();
    void setLiveMode();
    void setAdMode();
    void setStatusLabelLeft(const std::string& value);
    void setStatusLabelRight(const std::string& value);
    void disableCloseOnEndOfFile();
    void showVideoProgressSlider();
    void hideVideoProgressSlider();
    void disableProgressSliderSeek(bool disabled);
    void hideStatusLabel();
    void hideOSDLockButton();
    void hideHistorySetting() {}
    void hideVideoRelatedSetting() {}
    void hideSubtitleSetting() {}
    void hideBottomLineSetting() {}
    void hideHighlightLineSetting() {}
    void setFullscreenIcon(bool);
    void setFavoriteIcon(bool);
    void setFavoriteCallback(std::function<void(bool)> cb) { favoriteCb_ = std::move(cb); }
    void toggleFavorite();
    void setOnEndCallback(std::function<void()> cb) { onEndCb_ = std::move(cb); }
    void registerMpvEvent();
    void unRegisterMpvEvent();

    static View* create();
    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;
    View* getDefaultFocus() override;

    inline static const std::string SET_TITLE = "SET_TITLE";

private:
    aniswitch::MPVCore* mpvCore_ = nullptr;
    bool registerMPVEvent_ = false;
    bool is_osd_shown_ = false;
    bool is_osd_lock_ = false;
    bool closeOnEndOfFile_ = true;
    std::function<void()> onEndCb_;
    std::function<void(bool)> favoriteCb_;
    bool isFavorite_ = false;
    brls::Time osdLastShowTime_ = 0;
    MPVEvent::Subscription eventSubscribeID_;

    BRLS_BIND(brls::Label, videoTitleLabel, "video/osd/title");
    BRLS_BIND(brls::Box, osdTopBox, "video/osd/top/box");
    BRLS_BIND(brls::Box, osdBottomBox, "video/osd/bottom/box");
    BRLS_BIND(brls::Box, osdCenterBox, "video/osd/center/box");
    BRLS_BIND(brls::ProgressSpinner, osdSpinner, "video/osd/loading");
    BRLS_BIND(brls::Label, centerLabel, "video/osd/center/label");
    BRLS_BIND(brls::Box, osdCenterBox2, "video/osd/center/box2");
    BRLS_BIND(brls::Label, centerLabel2, "video/osd/center/label2");
    BRLS_BIND(SVGImage, centerIcon2, "video/osd/center/icon2");
    BRLS_BIND(VideoProgressSlider, osdSlider, "video/osd/bottom/progress");
    BRLS_BIND(brls::Label, leftStatusLabel, "video/left/status");
    BRLS_BIND(brls::Label, centerStatusLabel, "video/center/status");
    BRLS_BIND(brls::Label, rightStatusLabel, "video/right/status");
    BRLS_BIND(brls::Box, btnToggle, "video/osd/toggle");
    BRLS_BIND(SVGImage, btnToggleIcon, "video/osd/toggle/icon");
    BRLS_BIND(SVGImage, btnFullscreenIcon, "video/osd/fullscreen/icon");
    BRLS_BIND(SVGImage, btnFavoriteIcon, "video/osd/favorite/icon");
    BRLS_BIND(SVGImage, btnSettingIcon, "video/osd/setting/icon");
    BRLS_BIND(brls::Label, hintLabel, "video/osd/hint/label");
    BRLS_BIND(brls::Box, hintBox, "video/osd/hint/box");
    BRLS_BIND(VideoProfile, videoProfile, "video/profile");
    BRLS_BIND(brls::Box, osdLockBox, "video/osd/lock/box");
    BRLS_BIND(SVGImage, osdLockIcon, "video/osd/lock/icon");
};
