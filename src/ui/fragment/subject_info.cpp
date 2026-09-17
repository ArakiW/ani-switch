// SPDX-License-Identifier: AGPL-3.0
#include "ui/fragment/subject_info.hpp"

namespace aniswitch {
SubjectInfoFragment::SubjectInfoFragment(int32_t subjectId)
    : subjectId_(subjectId) {
    auto* lbl = new brls::Label();
    lbl->setText("番剧简介 (在 SubjectActivity 中渲染)");
    lbl->setFontSize(20);
    addView(lbl);
}
}
