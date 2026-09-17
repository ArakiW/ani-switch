// SPDX-License-Identifier: AGPL-3.0
#include "ui/fragment/season_evaluate.hpp"

namespace aniswitch {
SeasonEvaluateFragment::SeasonEvaluateFragment(int32_t subjectId)
    : subjectId_(subjectId) {
    auto* lbl = new brls::Label();
    lbl->setText("评论 (Phase 3)");
    lbl->setFontSize(20);
    addView(lbl);
}
}
