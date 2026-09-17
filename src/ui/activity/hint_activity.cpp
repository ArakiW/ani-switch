// SPDX-License-Identifier: AGPL-3.0

#include "ui/activity/hint_activity.hpp"
#include "utils/activity_helper.hpp"

namespace aniswitch {

HintActivity::HintActivity() = default;

void HintActivity::onContentAvailable() {
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setPadding(20);

    auto* title = new brls::Label();
    title->setText("ani-switch");
    title->setFontSize(36);
    title->setMarginBottom(20);
    root->addView(title);

    auto* body = new brls::Label();
    body->setText(
        "当前为相册小程序模式，内存不足以可靠播放视频。\n\n"
        "请返回主界面，按住 R 启动一个游戏进入完整 hbmenu，再启动 ani-switch。\n"
        "完整模式中可播放本地视频或 HTTP(S) 地址；账号登录不是播放前提。\n"
    );
    body->setFontSize(18);
    root->addView(body);

    auto* ok = new brls::Button();
    ok->setText("退出");
    ok->setMarginTop(20);
    ok->registerClickAction([](brls::View*) {
        brls::Application::quit();
        return true;
    });
    root->addView(ok);

    setContentView(root);
}

}  // namespace aniswitch
