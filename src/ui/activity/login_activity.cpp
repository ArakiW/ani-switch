// SPDX-License-Identifier: AGPL-3.0
#include "ui/activity/login_activity.hpp"
#include "ui/theme.hpp"
#include "net/bgm_auth.hpp"
#include "net/bgm_client.hpp"
#include "utils/config_helper.hpp"
#include "utils/activity_helper.hpp"
#include <borealis/core/thread.hpp>
#include <borealis/core/logger.hpp>

namespace aniswitch {

namespace {
// Mirrors the redirect_uri we ask the user to register at
// https://bgm.tv/dev/app.  The custom scheme means Bangumi will try
// to deep-link back into the app; on Switch the redirect fails (no
// system browser, no scheme handler), so the user just copies the
// `?code=` from the URL bar of the desktop browser into the in-app
// "粘贴授权码" field below.
constexpr const char* REDIRECT_URI = "ani-switch://oauth/callback";
}

LoginActivity::LoginActivity() = default;

void LoginActivity::onContentAvailable() {
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setPadding(20);

    auto* title = new brls::Label();
    title->setText("Bangumi 登录");
    title->setFontSize(28);
    root->addView(title);

    // Status line — filled with the PKCE authorize URL after the
    // user taps "PKCE 登录", so they can read + copy it.
    auto* status = new brls::Label();
    status->setText("方式一：PKCE（推荐）—  在桌面浏览器完成授权后回来粘贴 code。\n方式二：PAT —  自行生成 access_token 直接粘贴。");
    status->setFontSize(theme::kTypeCaption);
    status->setSingleLine(false);
    root->addView(status);

    auto* urlLabel = new brls::Label();
    urlLabel->setText("");
    urlLabel->setFontSize(theme::kTypeCaption);
    urlLabel->setSingleLine(false);
    root->addView(urlLabel);

    std::weak_ptr<int> lifetime = lifetime_;
    auto& cfg = ProgramConfig::instance();

    // ---- PKCE 登录 -----------------------------------------------------
    auto* pkceButton = new brls::Button();
    pkceButton->setText("1. PKCE 登录（生成授权链接）");
    pkceButton->registerClickAction([lifetime, urlLabel](brls::View*) {
        if (lifetime.expired()) return true;
        auto& c = ProgramConfig::instance();
        if (!c.hasBangumiClientCredentials()) {
            brls::Application::notify("ani-switch.json 缺少 bangumiClientId / bangumiClientSecret");
            return true;
        }
        const std::string state = "aniswitch";
        BangumiAuth::AuthorizationRequest req = BangumiAuth::buildAuthorizeUrl(
            c.getBangumiClientId(), REDIRECT_URI, state);
        // Stash the verifier on the singleton so the "paste code" step
        // below can pair it with the auth code the user pastes.
        c.setBangumiPKCEVerifier(req.codeVerifier);
        urlLabel->setText(req.url);
        brls::Application::notify("请在桌面浏览器打开上面的 URL 完成授权");
        return true;
    });
    root->addView(pkceButton);

    // ---- 粘贴 code ----------------------------------------------------
    auto* codeButton = new brls::Button();
    codeButton->setText("2. 粘贴授权码（PKCE 用）");
    codeButton->registerClickAction([lifetime](brls::View*) {
        if (lifetime.expired()) return true;
        auto& c = ProgramConfig::instance();
        if (!c.hasBangumiClientCredentials()) {
            brls::Application::notify("ani-switch.json 缺少 bangumiClientId / bangumiClientSecret");
            return true;
        }
        const std::string verifier = c.getBangumiPKCEVerifier();
        if (verifier.empty()) {
            brls::Application::notify("请先点上面的 \"1. PKCE 登录\"");
            return true;
        }
        auto* ime = brls::Application::getImeManager();
        if (!ime) return true;
        ime->openForText([lifetime, verifier](const std::string& code) {
            if (code.empty() || lifetime.expired()) return;
            auto& c2 = ProgramConfig::instance();
            BangumiAuth::exchangeCode(c2.getBangumiClientId(),
                                      c2.getBangumiClientSecret(),
                                      REDIRECT_URI, code, verifier,
                [lifetime](OAuthToken t) {
                    brls::sync([lifetime, t] {
                        if (lifetime.expired()) return;
                        ProgramConfig::instance().setBangumiToken(
                            t.accessToken, t.refreshToken, t.expiresIn, t.userId);
                        BangumiClient::setAccessToken(t.accessToken);
                        ProgramConfig::instance().clearBangumiPKCEVerifier();
                        brls::Application::notify("登录成功");
                        Intent::openMain();
                    });
                },
                [lifetime](const std::string& message, int) {
                    brls::sync([lifetime, message] {
                        if (!lifetime.expired()) brls::Application::notify("登录失败: " + message);
                    });
                });
        }, "Authorization code", "", 256);
        return true;
    });
    root->addView(codeButton);

    // ---- PAT fallback --------------------------------------------------
    auto* patButton = new brls::Button();
    patButton->setText("3. 粘贴 PAT（兼容）");
    patButton->registerClickAction([lifetime](brls::View*) {
        auto* ime = brls::Application::getImeManager();
        if (!ime) return true;
        ime->openForText([lifetime](const std::string& token) {
            if (token.empty() || lifetime.expired()) return;
            BangumiAuth::verifyPAT(token,
                [lifetime, token](std::string userId) {
                    brls::sync([lifetime, token, userId] {
                        if (lifetime.expired()) return;
                        ProgramConfig::instance().setBangumiToken(token, "", 0, userId);
                        BangumiClient::setAccessToken(token);
                        Intent::openMain();
                    });
                },
                [lifetime](const std::string& message, int) {
                    brls::sync([lifetime, message] {
                        if (!lifetime.expired()) brls::Application::notify("登录失败: " + message);
                    });
                });
        }, "Personal Access Token", "", 256);
        return true;
    });
    root->addView(patButton);

    auto* cancel = new brls::Button();
    cancel->setText("取消");
    cancel->setMarginTop(20);
    cancel->registerClickAction([](brls::View*) {
        brls::Application::popActivity();
        return true;
    });
    root->addView(cancel);
    setContentView(root);
}

}  // namespace aniswitch
