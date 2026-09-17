// SPDX-License-Identifier: AGPL-3.0
//
// v22 graft: TsVitch player shell. Full OSD VideoView from TsVitch
// (borealis XML) + ani-switch source download / resolve pipeline.
#pragma once

#include <borealis.hpp>
#include <memory>
#include <string>
#include <vector>

class VideoView;

namespace aniswitch {

class PlayerActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/player_tsvitch.xml");
    PlayerActivity(int32_t episodeId, const std::string& danmakuSource,
                   const std::string& videoSource,
                   int64_t resumePositionMs);
    ~PlayerActivity() override;
    void onContentAvailable() override;
    // v22: if XML inflate of VideoView fails, rebuild programmatically.
    void onContentAvailableFallback();

private:
    void startPlayback(const std::string& url);
    void downloadThenPlay(const std::string& url);

    int32_t episodeId_ = 0;
    int32_t subjectId_ = 0;
    std::string videoSource_;
    int64_t resumePositionMs_ = 0;
    std::vector<std::string> fallbackUrls_;
    bool subscribed_ = false;
    size_t subscription_ = 0;
    std::shared_ptr<int> lifetime_ = std::make_shared<int>(0);

    VideoView* video_ = nullptr;
};

}  // namespace aniswitch
