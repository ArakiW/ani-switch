// SPDX-License-Identifier: AGPL-3.0
//
// v17.4: Local cache / video file browser.  Lists everything the
// LocalVideoScanner finds on the SD card, grouped by source
// directory, and lets the user play any of them via the existing
// PlayerActivity (which already short-circuits when videoSource_
// is not "http" — see player_activity.cpp).

#pragma once

#include <borealis.hpp>
#include "core/local_video_scanner.hpp"
#include <memory>
#include <vector>

namespace aniswitch {

class LocalVideoActivity : public brls::Activity {
public:
    // Required: Activity::setContentView(nullptr) null-derefs.
    // Other screens ship a <brls:Box /> romfs placeholder.
    CONTENT_FROM_XML_RES("activity/local_video_activity.xml");
    LocalVideoActivity();
    ~LocalVideoActivity() override { lifetime_.reset(); }
    void onContentAvailable() override;

private:
    void renderList();

    std::vector<LocalVideoEntry> entries_;
    brls::Box* list_ = nullptr;
    brls::Label* statusLabel_ = nullptr;
    std::shared_ptr<int> lifetime_ = std::make_shared<int>(0);
};

}  // namespace aniswitch
