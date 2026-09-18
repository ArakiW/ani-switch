// SPDX-License-Identifier: AGPL-3.0
//
// Email OTP login — IME via ImeManager (EditTextDialog does not open
// swkbd reliably on Switch). Tokens still auto-save after OTP verify.

#include "ui/activity/email_login_activity.hpp"
#include "ui/theme.hpp"
#include "ui/ui_chrome.hpp"
#include "ui/hud.hpp"
#include "net/ani_client.hpp"
#include "net/bgm_client.hpp"
#include "utils/activity_helper.hpp"
#include "utils/config_helper.hpp"
#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>
#include <fmt/format.h>

#if defined(__SWITCH__)
extern "C" void aniswitchStartupLog(const char*);
#endif

namespace aniswitch {

EmailLoginActivity::EmailLoginActivity() = default;

void EmailLoginActivity::onContentAvailable() {
    root_ = chrome::makePageRoot();
    auto* shell = chrome::attachScrollShell(this, root_);
    chrome::registerBack(this);
    renderStart();
    chrome::finish(this, shell);
}

void EmailLoginActivity::renderStart() {
    while (!root_->getChildren().empty()) {
        root_->removeView(root_->getChildren().front());
    }

    root_->addView(chrome::makeTitle("邮箱登录 (ani)", theme::kTypeH1, 8));
    root_->addView(chrome::makeCaption(
        "输入邮箱, ani server 会发 6 位 OTP 到该邮箱。\n"
        "登录后 ani 会同步给你一个 Bangumi PAT, 一起填好两边。",
        20));

    statusLabel_ = new brls::Label();
    statusLabel_->setFontSize(theme::kTypeCaption);
    statusLabel_->setTextColor(theme::kDarkTextSecondary);
    statusLabel_->setSingleLine(false);
    statusLabel_->setMarginBottom(12);
    root_->addView(statusLabel_);

    root_->addView(chrome::makePrimaryButton(
        "输入邮箱发送验证码",
        [this]() {
            auto* ime = brls::Application::getImeManager();
            if (!ime) {
                statusLabel_->setText("系统输入法不可用");
                return;
            }
            ime->openForText(
                [this](std::string email) {
                    if (email.empty()) return;
                    if (email.find('@') == std::string::npos) {
                        statusLabel_->setText("请输入合法邮箱地址");
                        return;
                    }
                    statusLabel_->setText("发送中...");
                    pendingEmail_ = email;
#if defined(__SWITCH__)
                    aniswitchStartupLog("EMAIL: send otp");
#endif
                    AniClient::sendEmailOtp(
                        email,
                        [this](AniOtpResponse r) {
                            brls::sync([this, r]() {
                                pendingOtpId_ = r.otpId;
                                pendingHasExistingUser_ = r.hasExistingUser;
                                renderVerify(r.otpId, pendingEmail_,
                                             r.hasExistingUser);
                            });
                        },
                        [this](const std::string& msg, int code) {
                            brls::sync([this, msg, code]() {
                                statusLabel_->setText(
                                    fmt::format("发送失败 ({}): {}", code, msg));
                            });
                        });
                },
                "邮箱地址", "user@example.com", 128);
        },
        8));
}

void EmailLoginActivity::renderVerify(const std::string& otpId,
                                      const std::string& email,
                                      bool hasExistingUser) {
    while (!root_->getChildren().empty()) {
        root_->removeView(root_->getChildren().front());
    }

    root_->addView(chrome::makeTitle("输入 6 位 OTP", theme::kTypeH1, 8));
    root_->addView(chrome::makeCaption(
        fmt::format("已发送至 {}{}", email,
                    hasExistingUser ? " (老用户)" : " (新用户将自动注册)"),
        20));

    statusLabel_ = new brls::Label();
    statusLabel_->setFontSize(theme::kTypeCaption);
    statusLabel_->setTextColor(theme::kDarkTextSecondary);
    statusLabel_->setSingleLine(false);
    statusLabel_->setMarginBottom(12);
    root_->addView(statusLabel_);

    root_->addView(chrome::makePrimaryButton(
        "输入验证码",
        [this, otpId]() {
            auto* ime = brls::Application::getImeManager();
            if (!ime) {
                statusLabel_->setText("系统输入法不可用，请用设置页代理输入法路径重试");
                return;
            }
            const std::string otpIdCopy = otpId;
            ime->openForText(
                [this, otpIdCopy](std::string otp) {
                    if (otp.empty()) return;
                    while (!otp.empty() && (otp.back() == ' ' || otp.back() == '\n'))
                        otp.pop_back();
                    if (otp.size() != 6) {
                        statusLabel_->setText("OTP 长度不对, 应该是 6 位");
                        return;
                    }
                    statusLabel_->setText("验证中...");
#if defined(__SWITCH__)
                    aniswitchStartupLog("EMAIL: verify otp");
#endif
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
                                    cfg.bangumiAccessToken = r.tokens.bangumiPat;
                                    if (cfg.bangumiUserId.empty())
                                        cfg.bangumiUserId = r.userId;
                                    BangumiClient::setAccessToken(r.tokens.bangumiPat);
                                }
                                AniClient::setAccessToken(r.tokens.accessToken);
                                cfg.save();
#if defined(__SWITCH__)
                                aniswitchStartupLog("EMAIL: login ok");
#endif
                                brls::Application::notify("邮箱登录成功");
                                brls::Application::popActivity();
                            });
                        },
                        [this](const std::string& msg, int code) {
                            brls::sync([this, msg, code]() {
                                statusLabel_->setText(
                                    fmt::format("验证失败 ({}): {}", code, msg));
                            });
                        });
                },
                "6 位 OTP 验证码", "123456", 8);
        },
        8));

    root_->addView(chrome::makeSecondaryButton(
        "重新发送 / 换邮箱",
        [this]() { renderStart(); },
        8));
}

}  // namespace aniswitch
