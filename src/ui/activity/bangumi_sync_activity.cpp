// SPDX-License-Identifier: AGPL-3.0

#include "ui/activity/bangumi_sync_activity.hpp"
#include "ui/theme.hpp"
#include "net/bgm_auth.hpp"
#include "utils/config_helper.hpp"
#include <borealis/core/application.hpp>
#include <borealis/views/dialog.hpp>
#include <fmt/format.h>
#include <ctime>

namespace aniswitch {

BangumiSyncActivity::BangumiSyncActivity() = default;

void BangumiSyncActivity::onContentAvailable() {
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setPadding(20, 20, 20, 20);

    auto* title = new brls::Label();
    title->setText("Bangumi 同步");
    title->setFontSize(24);
    title->setMarginBottom(12);
    root->addView(title);

    statusLabel_ = new brls::Label();
    statusLabel_->setFontSize(16);
    statusLabel_->setTextColor(aniswitch::theme::kAccent);
    statusLabel_->setMarginBottom(6);
    root->addView(statusLabel_);

    detailLabel_ = new brls::Label();
    detailLabel_->setFontSize(theme::kTypeCaption);
    detailLabel_->setTextColor(aniswitch::theme::kDarkTextSecondary);
    detailLabel_->setSingleLine(false);
    detailLabel_->setMarginBottom(20);
    root->addView(detailLabel_);

    auto* refreshBtn = new brls::Button();
    refreshBtn->setText("立即刷新 token");
    refreshBtn->setMarginBottom(8);
    refreshBtn->registerClickAction([this](brls::View*) {
        presenter_.refreshNow();
        return true;
    });
    root->addView(refreshBtn);

    auto* logoutBtn = new brls::Button();
    logoutBtn->setText("退出登录");
    logoutBtn->setMarginBottom(8);
    logoutBtn->registerClickAction([](brls::View*) {
        auto* dlg = new brls::Dialog("确定要退出 Bangumi 登录?\n本地 token 会清空, 但不会撤销 server 端授权.");
        dlg->setCancelable(true);
        dlg->addButton("取消", []() {});
        dlg->addButton("退出", []() {
            BangumiAuth::logout();
            brls::Application::popActivity();
        });
        dlg->open();
        return true;
    });
    root->addView(logoutBtn);

    setContentView(root);

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
