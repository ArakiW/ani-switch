// SPDX-License-Identifier: AGPL-3.0
//
// v22.1 embedded Bangumi login: in-app cards + auto token save +
// auto fetch /v0/users/me after success (no separate refresh step).
// Switch has no system browser — PKCE still needs a desktop/phone
// for the authorize page; the in-app half is fully embedded.
//
// v22.2: PKCE code / PAT input via ImeManager::openForText (system
// swkbd). EditTextDialog fails to open the Switch IME reliably.
// HOME LED is playback-scoped — login no longer touches it.

#include "ui/activity/login_activity.hpp"
#include "ui/theme.hpp"
#include "ui/ui_chrome.hpp"
#include "ui/hud.hpp"
#include "net/bgm_auth.hpp"
#include "net/bgm_client.hpp"
#include "net/http.hpp"
#include "utils/config_helper.hpp"
#include "utils/activity_helper.hpp"
#include <borealis/core/thread.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/core/application.hpp>
#include <cpr/cpr.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#if defined(__SWITCH__)
extern "C" void aniswitchStartupLog(const char*);
#endif

namespace aniswitch {

namespace {
constexpr const char* REDIRECT_URI = "ani-switch://oauth/callback";

// Fetch nickname/username from Bangumi after a token is stored.
void fetchSelfProfile(std::function<void(std::string userId, std::string nick)> ok,
                      std::function<void(std::string)> fail) {
    const std::string token = ProgramConfig::instance().getBangumiAccessToken();
    if (token.empty()) {
        if (fail) fail("无 access_token");
        return;
    }
    const std::string meUrl = BangumiClient::baseUrl() + "/v0/users/me";
    auto session = HTTP::createSession();
    HTTP::applyDnsToSession(*session, meUrl);
    session->SetUrl(cpr::Url{meUrl});
    session->SetHeader({
        {"User-Agent", "ani-switch/0.1.0"},
        {"Accept", "application/json"},
        {"Authorization", "Bearer " + token},
    });
    session->SetTimeout(cpr::Timeout{15000});
    session->GetCallback([ok, fail](const cpr::Response& r) {
        if (r.error) {
            if (fail) fail(r.error.message);
            return;
        }
        if (r.status_code != 200) {
            // Older PAT path used /v0/me — try once more.
            if (fail) fail(fmt::format("HTTP {}", r.status_code));
            return;
        }
        try {
            auto j = nlohmann::json::parse(r.text);
            std::string id, nick, username;
            if (j.contains("id")) {
                if (j["id"].is_string()) id = j["id"].get<std::string>();
                else if (j["id"].is_number_integer())
                    id = std::to_string(j["id"].get<int64_t>());
            }
            if (j.contains("nickname") && j["nickname"].is_string())
                nick = j["nickname"].get<std::string>();
            if (j.contains("username") && j["username"].is_string())
                username = j["username"].get<std::string>();
            if (nick.empty()) nick = username;
            if (ok) ok(id, nick);
        } catch (const std::exception& e) {
            if (fail) fail(e.what());
        }
    });
}

void trimCode(std::string& code) {
    auto pos = code.find("code=");
    if (pos != std::string::npos) code = code.substr(pos + 5);
    while (!code.empty() && (code.back() == ' ' || code.back() == '\n' ||
                             code.back() == '\r' || code.back() == '\t'))
        code.pop_back();
    while (!code.empty() && (code.front() == ' ' || code.front() == '\n' ||
                             code.front() == '\r' || code.front() == '\t'))
        code.erase(code.begin());
}
}  // namespace

LoginActivity::LoginActivity() = default;

void LoginActivity::onContentAvailable() {
    auto* root = chrome::makePageRoot();

    root->addView(chrome::makeTitle("账号登录", theme::kTypeH1, 8));
    root->addView(chrome::makeCaption(
        "登录后自动保存 token 并拉取账号信息。\n"
        "Switch 无内置浏览器：PKCE 需在手机/电脑打开授权链接后，把 code 粘回本页。",
        16));

    status_ = chrome::makeCaption(statusText_.empty() ? "未登录" : statusText_, 12);
    root->addView(status_);

    std::weak_ptr<int> lifetime = lifetime_;

    auto showLoggedIn = [this, lifetime](const std::string& userId,
                                         const std::string& nick) {
        brls::sync([this, lifetime, userId, nick] {
            if (lifetime.expired() || !status_) return;
            statusText_ = fmt::format("已登录 · id {} · {}", userId,
                                      nick.empty() ? "Bangumi 用户" : nick);
            status_->setText(statusText_);
            status_->setTextColor(theme::kSuccess);
        });
    };

    auto afterTokenSaved = [this, lifetime, showLoggedIn](const char* tag) {
        // Auto profile pull — no extra button.
        fetchSelfProfile(
            [lifetime, showLoggedIn, tag](std::string id, std::string nick) {
                if (lifetime.expired()) return;
                if (id.empty()) {
                    const std::string cfgId = ProgramConfig::instance().getUserID();
                    id = cfgId.empty() ? std::string("?") : cfgId;
                }
                brls::Logger::info("LOGIN: ok via {} user={} nick={}", tag, id,
                                   nick);
#if defined(__SWITCH__)
                {
                    std::string m = fmt::format("LOGIN: ok via {} user={}", tag, id);
                    aniswitchStartupLog(m.c_str());
                }
#endif
                showLoggedIn(id, nick);
                brls::Application::notify(
                    fmt::format("登录成功{}", nick.empty() ? "" : (": " + nick)));
            },
            [this, lifetime, showLoggedIn, tag](const std::string& err) {
                if (lifetime.expired()) return;
                // Token may still be valid; surface id from config.
                const std::string uid = ProgramConfig::instance().getUserID();
                brls::Logger::warning("LOGIN: profile fail via {} {}", tag, err);
                if (!uid.empty()) {
                    showLoggedIn(uid, "");
                    brls::Application::notify("已保存 token（资料拉取失败: " + err + "）");
                } else {
                    if (status_) {
                        status_->setText("token 已保存，资料拉取失败: " + err);
                        status_->setTextColor(theme::kWarning);
                    }
                }
            });
    };

    // ---- 邮箱 OTP (ani) -------------------------------------------------
    root->addView(chrome::makeSection("方式一：邮箱 OTP（推荐，应用内完成）", 8));
    root->addView(chrome::makeMuted(
        "输入邮箱 → 收 6 位验证码 → 粘贴。ani 会同步 Bangumi PAT 并自动写入配置。",
        8));
    root->addView(chrome::makePrimaryButton(
        "邮箱登录 (ani OTP)",
        []() { Intent::openEmailLoginStart(); },
        12));

    // ---- PKCE ------------------------------------------------------------
    root->addView(chrome::makeSection("方式二：PKCE 授权（浏览器 + 粘贴 code）", 8));

    auto* urlLabel = chrome::makeMuted("", 8);
    auto* pkceBtn = chrome::makePrimaryButton(
        "生成授权链接",
        [lifetime, urlLabel]() {
            if (lifetime.expired()) return;
            auto& c = ProgramConfig::instance();
            if (!c.hasBangumiClientCredentials()) {
                brls::Application::notify(
                    "ani-switch.json 缺少 bangumiClientId / bangumiClientSecret");
                return;
            }
            const std::string state = "aniswitch";
            BangumiAuth::AuthorizationRequest req = BangumiAuth::buildAuthorizeUrl(
                c.getBangumiClientId(), REDIRECT_URI, state);
            c.setBangumiPKCEVerifier(req.codeVerifier);
            urlLabel->setText(req.url);
            brls::Application::notify("请在手机/电脑打开上面链接，授权后复制 ?code=");
        },
        8);
    root->addView(pkceBtn);
    root->addView(urlLabel);

    root->addView(chrome::makeSecondaryButton(
        "粘贴授权码并自动登录",
        [lifetime, afterTokenSaved]() {
            if (lifetime.expired()) return;
            auto& c = ProgramConfig::instance();
            const std::string verifier = c.getBangumiPKCEVerifier();
            if (verifier.empty()) {
                brls::Application::notify("请先点「生成授权链接」");
                return;
            }
            auto* ime = brls::Application::getImeManager();
            if (!ime) {
                brls::Application::notify("系统输入法不可用");
                return;
            }
            ime->openForText(
                [lifetime, verifier, afterTokenSaved](std::string code) {
                    if (lifetime.expired()) return;
                    trimCode(code);
                    if (code.empty()) return;
                    auto& c2 = ProgramConfig::instance();
                    BangumiAuth::exchangeCode(
                        c2.getBangumiClientId(), c2.getBangumiClientSecret(),
                        REDIRECT_URI, code, verifier,
                        [lifetime, afterTokenSaved](OAuthToken t) {
                            brls::sync([lifetime, t, afterTokenSaved] {
                                if (lifetime.expired()) return;
                                ProgramConfig::instance().setBangumiToken(
                                    t.accessToken, t.refreshToken, t.expiresIn,
                                    t.userId);
                                BangumiClient::setAccessToken(t.accessToken);
                                ProgramConfig::instance().clearBangumiPKCEVerifier();
                                afterTokenSaved("pkce");
                            });
                        },
                        [lifetime](const std::string& message, int) {
                            brls::sync([lifetime, message] {
                                if (!lifetime.expired())
                                    brls::Application::notify("登录失败: " + message);
                            });
                        });
                },
                "粘贴 Authorization code", "oauth redirect ?code=....", 255);
        },
        12));

    // ---- PAT -------------------------------------------------------------
    root->addView(chrome::makeSection("方式三：PAT（个人访问令牌）", 8));
    root->addView(chrome::makeMuted(
        "在 bgm.tv 开发者页生成 PAT 后粘贴，将自动校验并拉取账号信息。", 8));
    root->addView(chrome::makeSecondaryButton(
        "粘贴 PAT 并自动登录",
        [lifetime, afterTokenSaved]() {
            auto* ime = brls::Application::getImeManager();
            if (!ime) {
                brls::Application::notify("系统输入法不可用");
                return;
            }
            ime->openForText(
                [lifetime, afterTokenSaved](std::string token) {
                    if (lifetime.expired()) return;
                    while (!token.empty() && (token.back() == ' ' ||
                                               token.back() == '\n' ||
                                               token.back() == '\r'))
                        token.pop_back();
                    if (token.empty()) return;
                    BangumiAuth::verifyPAT(
                        token,
                        [lifetime, token, afterTokenSaved](std::string userId) {
                            brls::sync([lifetime, token, userId, afterTokenSaved] {
                                if (lifetime.expired()) return;
                                ProgramConfig::instance().setBangumiToken(token, "", 0,
                                                                           userId);
                                BangumiClient::setAccessToken(token);
                                afterTokenSaved("pat");
                            });
                        },
                        [lifetime](const std::string& message, int) {
                            brls::sync([lifetime, message] {
                                if (!lifetime.expired())
                                    brls::Application::notify("PAT 无效: " + message);
                            });
                        });
                },
                "Personal Access Token", "bgm.tv PAT", 255);
        },
        12));

    // Already logged in?
    if (ProgramConfig::instance().hasLoginInfo()) {
        const std::string uid = ProgramConfig::instance().getUserID();
        statusText_ = fmt::format("配置中已有登录: user {}", uid);
        if (status_) status_->setText(statusText_);
        fetchSelfProfile(
            [this, lifetime, showLoggedIn, uid](std::string id, std::string nick) {
                if (lifetime.expired()) return;
                showLoggedIn(id.empty() ? uid : id, nick);
            },
            [this, lifetime](const std::string& err) {
                if (lifetime.expired() || !status_) return;
                status_->setText("已有 token，资料拉取失败: " + err);
                status_->setTextColor(theme::kDarkTextMuted);
            });
    }

    auto* shell = chrome::attachScrollShell(this, root);
    chrome::registerBack(this);
    chrome::finish(this, shell);
}

}  // namespace aniswitch
