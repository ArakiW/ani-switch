// SPDX-License-Identifier: AGPL-3.0
#include "ui/fragment/comment_list.hpp"

namespace aniswitch {
CommentListFragment::CommentListFragment(int32_t subjectId)
    : subjectId_(subjectId) {
    auto* lbl = new brls::Label();
    lbl->setText("评论 (Bangumi API 暂未公开,Phase 3)");
    lbl->setFontSize(20);
    addView(lbl);
}
}
