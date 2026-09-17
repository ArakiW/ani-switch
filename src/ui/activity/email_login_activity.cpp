// SPDX-License-Identifier: AGPL-3.0

#include "ui/activity/email_login_activity.hpp"
#include "ui/theme.hpp"
#include "net/ani_client.hpp"
#include "net/bgm_client.hpp"
#include "utils/activity_helper.hpp"
#include "utils/config_helper.hpp"
#include <borealis/core/application.hpp>
#include <borealis/views/edit_text_dialog.hpp>
#include <fmt/format.h>

namespace aniswitch {

EmailLoginActivity::EmailLoginActivity() = default;

void EmailLoginActivity::onContentAvailable() {
    root_ = new brls::Box();
    root_->setAxis(brls::Axis::COLUMN);
    root_->setPadding(40, 30, 40, 20);
    setContentView(root_);
    renderStart();
}

void EmailLoginActivity::renderStart() {
    while (!root_->getChildren().empty()) {
        root_->removeView(root_->getChildren().front());
    }

    auto* title = new brls::Label();
    title->setText("邮箱登录 (ani)");
    title->setFontSize(26);
    title->setMarginBottom(8);
    root_->addView(title);

    auto* hint = new brls::Label();
    hint->setText("输入邮箱, ani server 会发 6 位 OTP 到该邮箱。\n"
                  "登录后 ani 会同步给你一个 Bangumi PAT, 一起填好两边。");
    hint->setFontSize(theme::kTypeCaption);
    hint->setTextColor(aniswitch::theme::kDarkTextSecondary);
    hint->setSingleLine(false);
    hint->setMarginBottom(20);
    root_->addView(hint);

    statusLabel_ = new brls::Label();
    statusLabel_->setFontSize(theme::kTypeCaption);
    statusLabel_->setTextColor(aniswitch::theme::kAccent);
    statusLabel_->setMarginBottom(12);
    root_->addView(statusLabel_);

    auto* sendBtn = new brls::Button();
    sendBtn->setText("输入邮箱发送验证码");
    sendBtn->registerClickAction([this](brls::View*) {
        auto* dlg = new brls::EditTextDialog();
        dlg->setHeaderText("邮箱地址");
        dlg->setHintText("user@example.com");
        dlg->getSubmitEvent()->subscribe([this, dlg](...) {
            // borealis 5f08b286's getSubmitEvent() is Event<>* with
            // no payload.  The user-typed text is in the label
            // bound at id "brls/dialog/label", which is private
            // (BRLS_BIND-declared) on EditTextDialog.  We resolve
            // it through the Activity-side getView() helper, which
            // walks the inflated XML tree to find the bound child
            // by id.  The submit callback fires *before* the
            // popActivity frees the dialog, so the read is safe.
            auto* labelView = dynamic_cast<brls::Label*>(
                dlg->getView("brls/dialog/label"));
            std::string email = labelView ? labelView->getFullText() : "";
            // Lightweight sanity check; the server will still
            // 400 if it's malformed.
            if (email.empty() || email.find('@') == std::string::npos) {
                statusLabel_->setText("请输入合法邮箱地址");
                return;
            }
            statusLabel_->setText("发送中...");
            pendingEmail_ = email;
            AniClient::sendEmailOtp(
                email,
                [this](AniOtpResponse r) {
                    brls::sync([this, r]() {
                        pendingOtpId_ = r.otpId;
                        pendingHasExistingUser_ = r.hasExistingUser;
                        renderVerify(r.otpId, pendingEmail_, r.hasExistingUser);
                    });
                },
                [this](const std::string& msg, int code) {
                    brls::sync([this, msg, code]() {
                        statusLabel_->setText(
                            fmt::format("发送失败 ({}): {}", code, msg));
                    });
                });
        });
        dlg->open();
        return true;
    });
    root_->addView(sendBtn);
}

void EmailLoginActivity::renderVerify(const std::string& otpId,
                                     const std::string& email,
                                     bool hasExistingUser) {
    while (!root_->getChildren().empty()) {
        root_->removeView(root_->getChildren().front());
    }

    auto* title = new brls::Label();
    title->setText("输入 6 位 OTP");
    title->setFontSize(26);
    title->setMarginBottom(8);
    root_->addView(title);

    auto* info = new brls::Label();
    info->setText(fmt::format("已发送至 {}{}",
                              email,
                              hasExistingUser ? " (老用户)" : " (新用户将自动注册)"));
    info->setFontSize(theme::kTypeCaption);
    info->setTextColor(aniswitch::theme::kDarkTextSecondary);
    info->setSingleLine(false);
    info->setMarginBottom(20);
    root_->addView(info);

    statusLabel_ = new brls::Label();
    statusLabel_->setFontSize(theme::kTypeCaption);
    statusLabel_->setTextColor(aniswitch::theme::kAccent);
    statusLabel_->setMarginBottom(12);
    root_->addView(statusLabel_);

    auto* verifyBtn = new brls::Button();
    verifyBtn->setText("输入验证码");
    verifyBtn->registerClickAction([this, otpId](brls::View*) {
        auto* dlg = new brls::EditTextDialog();
        dlg->setHeaderText("6 位 OTP 验证码");
        dlg->setHintText("123456");
        std::string otpIdCopy = otpId;
        dlg->getSubmitEvent()->subscribe([this, dlg, otpIdCopy](...) {
            // Same hack as the email field — resolve the bound
            // Label by id through Activity::getView() and pull
            // the typed text out via Label::getFullText().
            auto* labelView = dynamic_cast<brls::Label*>(
                dlg->getView("brls/dialog/label"));
            std::string otp = labelView ? labelView->getFullText() : "";
            if (otp.size() != 6) {
                statusLabel_->setText("OTP 长度不对, 应该是 6 位");
                return;
            }
            statusLabel_->setText("验证中...");
            AniClient::loginByEmailOtp(
                otpIdCopy, otp,
                [this](AniLoginResponse r) {
                    brls::sync([this, r]() {
                        auto& cfg = ProgramConfig::instance();
                        cfg.aniAccessToken     = r.tokens.accessToken;
                        cfg.aniRefreshToken    = r.tokens.refreshToken;
                        cfg.aniExpiresAtMillis = r.tokens.expiresAtMillis;
                        cfg.aniUserId          = r.userId;
                        if (!r.tokens.bangumiPat.empty()) {
                            // ani server hands us a Bangumi PAT as
                            // a side-effect of email login — pull
                            // it into the Bangumi token slot so the
                            // user doesn't have to log in twice.
                            cfg.bangumiAccessToken = r.tokens.bangumiPat;
                            if (cfg.bangumiUserId.empty()) cfg.bangumiUserId = r.userId;
                            BangumiClient::setAccessToken(r.tokens.bangumiPat);
                        }
                        AniClient::setAccessToken(r.tokens.accessToken);
                        cfg.save();
                        brls::Application::popActivity();
                    });
                },
                [this](const std::string& msg, int code) {
                    brls::sync([this, msg, code]() {
                        statusLabel_->setText(
                            fmt::format("验证失败 ({}): {}", code, msg));
                    });
                });
        });
        dlg->open();
        return true;
    });
    root_->addView(verifyBtn);
}

}  // namespace aniswitch
