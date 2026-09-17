// SPDX-License-Identifier: AGPL-3.0
//
// v22: play-source picker (animeko-style). Episode tap opens this
// page first: resolve sources → show list + loading state → user
// picks one → Player starts that URL.
#pragma once

#include <borealis.hpp>
#include <string>
#include <vector>
#include "core/source_manager.hpp"

namespace aniswitch {

class SourcePickerActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/source_picker_activity.xml");
    SourcePickerActivity(int32_t episodeId,
                         int32_t subjectId = 0,
                         const std::string& title = "",
                         bool autoPickFirst = false);
    void onContentAvailable() override;

private:
    void setLoading(const std::string& msg);
    void renderSources(std::vector<VideoSource> sources);

    int32_t episodeId_ = 0;
    int32_t subjectId_ = 0;
    std::string title_;
    bool autoPickFirst_ = false;
    brls::Label* status_ = nullptr;
    brls::Box* list_ = nullptr;
    std::shared_ptr<int> lifetime_ = std::make_shared<int>(0);
};

}  // namespace aniswitch
