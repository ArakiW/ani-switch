// SPDX-License-Identifier: AGPL-3.0
#include "ui/fragment/player_fragments.hpp"

namespace aniswitch {
DanmakuInputFragment::DanmakuInputFragment() {
    auto* lbl = new brls::Label();
    lbl->setText("弹幕输入 (Phase 4)");
    lbl->setFontSize(20);
    addView(lbl);
}
QualitySelectFragment::QualitySelectFragment() {
    auto* lbl = new brls::Label();
    lbl->setText("画质选择 (Phase 2,等奶油蛋糕数据源)");
    lbl->setFontSize(20);
    addView(lbl);
}
SpeedSelectFragment::SpeedSelectFragment() {
    auto* lbl = new brls::Label();
    lbl->setText("倍速选择 (MPVCore 已有,接入 UI 在 Phase 2)");
    lbl->setFontSize(20);
    addView(lbl);
}
}
