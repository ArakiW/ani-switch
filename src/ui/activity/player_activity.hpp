// SPDX-License-Identifier: AGPL-3.0
#pragma once
#include <borealis.hpp>
#include <memory>
#include "utils/event_helper.hpp"

namespace aniswitch {
class PlayerActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/player_activity.xml");
    PlayerActivity(int32_t episodeId, const std::string& danmakuSource = "dandanplay",
                   const std::string& videoSource = "http", int64_t resumePositionMs = 0);
    ~PlayerActivity() override;
    void onContentAvailable() override;
    void onPause() override;
private:
    void start(const std::string& path);
    // v22: fetch progressive video via HTTP stack (fake-ip safe), then play.
    void downloadThenPlay(const std::string& url);
    void onPlayerEvent(MpvEventEnum event);
    void checkpoint();
    int32_t episodeId_;
    int32_t subjectId_ = 0;
    std::string videoSource_;
    int64_t resumePositionMs_;
    int64_t lastCheckpoint_ = -1;
    brls::Label* status_ = nullptr;
    MPVEvent::Subscription subscription_{};
    bool subscribed_ = false;
    bool started_ = false;
    std::shared_ptr<int> lifetime_ = std::make_shared<int>(0);
    // v22: remaining online sources to try if the current URL fails.
    std::vector<std::string> fallbackUrls_;
};
}
