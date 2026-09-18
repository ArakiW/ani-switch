// SPDX-License-Identifier: AGPL-3.0

#include "ui/activity/hint_activity.hpp"
#include "ui/theme.hpp"
#include "ui/ui_chrome.hpp"
#include "ui/hud.hpp"
#include "utils/activity_helper.hpp"
#include <borealis/core/application.hpp>

namespace aniswitch {

HintActivity::HintActivity() = default;

void HintActivity::onContentAvailable() {
    auto* root = chrome::makePageRoot();

    root->addView(chrome::makeTitle("ani-switch", theme::kTypeDisplay, 20));

    root->addView(chrome::makeBody(
        "当前为相册小程序模式，内存不足以可靠播放视频。\n\n"
        "请返回主界面，按住 R 启动一个游戏进入完整 hbmenu，再启动 ani-switch。\n"
        "完整模式中可播放本地视频或 HTTP(S) 地址；账号登录不是播放前提。\n",
        24));

    // Modal hint — no back stack. Primary action = quit applet mode.
    root->addView(chrome::makePrimaryButton(
        "退出",
        []() { brls::Application::quit(); },
        8));

    // HUD shell still present; A maps to the quit action so the
    // ActionMap chips match the on-screen primary button.
    auto* shell = chrome::attachScrollShell(this, root);
    registerAction("退出", brls::BUTTON_A, [](brls::View*) {
        brls::Application::quit();
        return true;
    });
    chrome::finish(this, shell);
}

}  // namespace aniswitch
