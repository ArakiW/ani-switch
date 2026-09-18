// SPDX-License-Identifier: AGPL-3.0

#include "ui/activity/bangumi_sync_activity.hpp"
#include "ui/theme.hpp"
#include "ui/ui_chrome.hpp"
#include "ui/hud.hpp"
#include "net/bgm_auth.hpp"
#include "utils/config_helper.hpp"
#include <borealis/core/application.hpp>
#include <borealis/views/dialog.hpp>
#include <fmt/format.h>
#include <ctime>

namespace aniswitch {

BangumiSyncActivity::BangumiSyncActivity() = default;

void BangumiSyncActivity::onContentAvailable() {
    auto* root = chrome::makePageRoot();

    root->addView(chrome::makeTitle("Bangumi 同步", theme::kTypeH1, 8));

    statusLabel_ = new brls::Label();
    statusLabel_->setFontSize(theme::kTypeH3);
    statusLabel_->setTextColor(theme::kDarkTextPrimary);
    statusLabel_->setSingleLine(false);
    statusLabel_->setMarginBottom(6);
    root->addView(statusLabel_);

    detailLabel_ = new brls::Label();
    detailLabel_->setFontSize(theme::kTypeCaption);
    detailLabel_->setTextColor(theme::kDarkTextSecondary);
    detailLabel_->setSingleLine(false);
    detailLabel_->setMarginBottom(20);
    root->addView(detailLabel_);

    root->addView(chrome::makeSection("账号操作", 8));

    // Primary first — token refresh is the main action on this page.
    root->addView(chrome::makePrimaryButton(
        "立即刷新 token",
        [this]() {
            presenter_.refreshNow();
        },
        8));

    root->addView(chrome::makeSecondaryButton(
        "退出登录",
        []() {
            auto* dlg = new brls::Dialog(
                "确定要退出 Bangumi 登录?\n本地 token 会清空, 但不会撤销 server 端授权.");
            dlg->setCancelable(true);
            dlg->addButton("取消", []() {});
            dlg->addButton("退出", []() {
                BangumiAuth::logout();
                brls::Application::popActivity();
            });
            dlg->open();
        },
        8));

    auto* shell = chrome::attachScrollShell(this, root);
    chrome::registerBack(this);
    chrome::finish(this, shell);

    presenter_.onResult.subscribe([this](std::string s) {
        brls::sync([this, s = std::move(s)]() {
            statusLabel_->setText(s);
            renderState();
        });
    });

    renderState();
}

void BangumiSyncActivity::renderState() {
    auto& cfg = ProgramConfig::instance();
    const std::string at = cfg.getBangumiAccessToken();
    const std::string rt = cfg.getBangumiRefreshToken();
    const std::string uid = cfg.getUserID();
    const int64_t exp = cfg.getBangumiTokenExpiry();

    std::string line1;
    if (at.empty()) {
        line1 = "未登录";
    } else {
        line1 = "已登录";
    }
    statusLabel_->setText(line1);

    std::string line2;
    if (at.empty()) {
        line2 = "访问 token: 无\n刷新 token: 无\n用户 ID: 无\n过期时间: 无";
    } else {
        // Bangumi access tokens live 6-7 days; we re-issue via the
        // periodic TokenRefreshTask.  Show the wall-clock time so
        // the user can tell whether the next background refresh is
        // going to land before the next time they open the app.
        int64_t now = static_cast<int64_t>(time(nullptr));
        int64_t remain = exp - now;
        std::string expStr = exp > 0
            ? fmt::format("{} ({} 秒后)", std::string(ctime(&exp)), remain)
            : "未知";
        // Truncate the trailing newline ctime() tacks on.
        if (!expStr.empty() && expStr.back() == '\n') expStr.pop_back();
        line2 = fmt::format(
            "访问 token: {}…\n刷新 token: {}…\n用户 ID: {}\n过期时间: {}",
            at.substr(0, std::min<size_t>(at.size(), 8)),
            rt.empty() ? "无" : rt.substr(0, std::min<size_t>(rt.size(), 8)),
            uid.empty() ? "无" : uid,
            expStr);
    }
    detailLabel_->setText(line2);
}

}  // namespace aniswitch
