// SPDX-License-Identifier: AGPL-3.0
//
// Episode list activity. Same data as subject_activity's episode tab,
// but as a standalone screen for callers that want to skip the
// subject detail (e.g. "Continue watching" deep links).

#pragma once

#include <borealis.hpp>
#include "ui/presenter/episode_presenter.hpp"

namespace aniswitch {

class EpisodeListActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/episode_list_activity.xml");
    EpisodeListActivity(int32_t subjectId, int32_t resumeEpisodeId = 0);
    void onContentAvailable() override;

private:
    int32_t subjectId_;
    int32_t resumeEpisodeId_;
    EpisodePresenter presenter_;
    brls::Box* list_ = nullptr;
    void render(const std::vector<Episode>& eps);
};

}  // namespace aniswitch
