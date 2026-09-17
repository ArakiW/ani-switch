// SPDX-License-Identifier: AGPL-3.0
#include "ui/fragment/episode_list.hpp"

namespace aniswitch {
EpisodeListFragment::EpisodeListFragment(int32_t subjectId)
    : subjectId_(subjectId) {
    auto* lbl = new brls::Label();
    lbl->setText("剧集列表 (在 SubjectActivity 中渲染)");
    lbl->setFontSize(20);
    addView(lbl);
}
}
