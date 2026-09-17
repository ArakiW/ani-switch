// SPDX-License-Identifier: AGPL-3.0
//
// ani-switch PATCH (2026-09-05): vendored borealis 5f08b286 doesn't have
// brls::Toggle (it's a newer API). Replace the toggle rows with simple
// Button rows that flip the underlying config item on each click.

#include "ui/activity/setting_activity.hpp"
#include "ui/theme.hpp"
#include "net/http.hpp"
#include "utils/activity_helper.hpp"
#include "utils/config_helper.hpp"
#include "utils/version_helper.hpp"
#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/views/scrolling_frame.hpp>
#include <fmt/format.h>

#if defined(__SWITCH__) && defined(ANISWITCH_SWITCH_DEBUG)
extern "C" void aniswitchStartupLog(const char*);
#define SET_TRACE(msg) aniswitchStartupLog(msg)
#else
#define SET_TRACE(msg) do { (void)0; } while (0)
#endif

namespace aniswitch {

namespace {
    brls::Button* makeToggleButton(SettingItem item, const std::string& label, ProgramConfig& cfg) {
        auto* btn = new brls::Button();
        const auto option = cfg.getOptionData(item);
        auto currentIndex = [item, &cfg, option]() {
            int index = cfg.getSettingItem<int>(item, static_cast<int>(option.defaultOption));
            if (index < 0 || index >= static_cast<int>(option.optionList.size()))
                index = static_cast<int>(option.defaultOption);
            return index;
        };
        auto refresh = [btn, label, option, currentIndex]() {
            btn->setText(fmt::format("{}: {}", label, option.optionList.at(currentIndex())));
        };
        btn->registerClickAction([item, &cfg, option, currentIndex, refresh](brls::View*) {
            cfg.setSettingItem(item, (currentIndex() + 1) % static_cast<int>(option.optionList.size()));
            refresh();
            return true;
        });
        refresh();
        btn->setHeight(theme::kButtonHeight);
        btn->setMarginBottom(8);
        theme::applyFocusStyle(btn);
        return btn;
    }

    // v17.0: theme toggle.  Cycles Auto → Light → Dark → Auto and
    // immediately re-applies the chosen theme so the user sees
    // the change without restarting.  Saves to ProgramConfig
    // so the choice is persisted across launches.
    brls::Button* makeThemeButton(ProgramConfig& cfg) {
        auto* btn = new brls::Button();
        const auto option = cfg.getOptionData(SettingItem::APP_THEME);
        auto currentIndex = [&cfg, option]() {
            int index = cfg.getSettingItem<int>(SettingItem::APP_THEME,
                                              static_cast<int>(option.defaultOption));
            if (index < 0 || index >= static_cast<int>(option.optionList.size()))
                index = static_cast<int>(option.defaultOption);
            return index;
        };
        auto refresh = [btn, option, currentIndex]() {
            btn->setText(fmt::format("主题: {}",
                                     option.optionList.at(currentIndex())));
        };
        btn->registerClickAction(
            [&cfg, option, currentIndex, refresh](brls::View*) {
                int next = (currentIndex() + 1) % static_cast<int>(option.optionList.size());
                cfg.setSettingItem(SettingItem::APP_THEME, next);
                aniswitch::theme::applyTheme(
                    aniswitch::theme::themeChoiceFromIndex(next));
                refresh();
                return true;
            });
        refresh();
        btn->setHeight(theme::kButtonHeight);
        btn->setMarginBottom(8);
        theme::applyFocusStyle(btn);
        return btn;
    }
}  // namespace

SettingActivity::SettingActivity() = default;

void SettingActivity::onContentAvailable() {
    SET_TRACE("SET: begin");
    auto& cfg = ProgramConfig::instance();

    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setPadding(16, theme::kSafeMarginX, 24, theme::kSafeMarginX);

    auto* header = new brls::Box();
    header->setAxis(brls::Axis::COLUMN);
    header->setHeight(72);
    header->setMarginBottom(12);
    auto* title = new brls::Label();
    title->setText("设置");
    title->setFontSize(theme::kTypeH2);
    title->setSingleLine(true);
    title->setHeight(36);
    header->addView(title);
    auto* authLabel = new brls::Label();
    authLabel->setText(cfg.hasLoginInfo()
        ? fmt::format("已登录: user {}", cfg.getUserID())
        : "未登录");
    authLabel->setFontSize(theme::kTypeH3);
    authLabel->setSingleLine(true);
    authLabel->setHeight(32);
    header->addView(authLabel);
    root->addView(header);

    auto* btnLogin = new brls::Button();
    btnLogin->setText(cfg.hasLoginInfo() ? "重新登录" : "登录 Bangumi");
    btnLogin->setHeight(theme::kButtonHeight);
    theme::applyFocusStyle(btnLogin);
    btnLogin->setMarginBottom(20);
    btnLogin->registerClickAction([](brls::View*) {
        Intent::openLogin();
        return true;
    });
    root->addView(btnLogin);

    // Player toggles (Button-as-toggle stand-in)
    root->addView(makeToggleButton(SettingItem::PLAYER_HWDEC,        "硬件解码",      cfg));
    root->addView(makeToggleButton(SettingItem::DANMAKU_ON,         "启用弹幕",      cfg));

    // v17.0: theme switcher.  Sits next to the player toggles so
    // it's easy to find without making a separate "appearance"
    // sub-screen.  applyTheme() inside the click handler does
    // the live re-skin; ProgramConfig::save() persists the
    // choice on disk.
    root->addView(makeThemeButton(cfg));

    // v20.11: LAN HTTP proxy (Clash Allow LAN on the PC).
    auto* proxyLabel = new brls::Label();
    proxyLabel->setText("网络代理");
    proxyLabel->setFontSize(theme::kTypeH3);
    proxyLabel->setSingleLine(true);
    proxyLabel->setMarginTop(16);
    proxyLabel->setMarginBottom(8);
    root->addView(proxyLabel);

    auto* proxyBtn = new brls::Button();
    auto* proxyHint = new brls::Label();
    proxyHint->setFontSize(theme::kTypeCaption);
    proxyHint->setSingleLine(false);
    proxyHint->setMarginBottom(8);
    auto refreshProxy = [proxyBtn, proxyHint]() {
        const auto p = ProgramConfig::instance().getProxy();
        proxyBtn->setText(p.empty() ? std::string("设置代理 (当前: 无)")
                                    : ("设置代理 (当前: " + p + ")"));
        proxyHint->setText(HTTP::proxyHint());
    };
    refreshProxy();
    proxyBtn->setHeight(theme::kButtonHeight);
    theme::applyFocusStyle(proxyBtn);
    proxyBtn->setMarginBottom(6);
    proxyBtn->registerClickAction([refreshProxy](brls::View*) {
        auto* ime = brls::Application::getImeManager();
        if (!ime) return true;
        ime->openForText(
            [refreshProxy](std::string text) {
                ProgramConfig::instance().setProxy(text);
                refreshProxy();
            },
            "HTTP 代理", "http://192.168.0.122:7897", 128);
        return true;
    });
    root->addView(proxyBtn);
    root->addView(proxyHint);
    auto* clearProxy = new brls::Button();
    clearProxy->setText("清除代理 (改回直连)");
    clearProxy->setHeight(theme::kButtonHeight);
    theme::applyFocusStyle(clearProxy);
    clearProxy->setMarginBottom(8);
    clearProxy->registerClickAction([refreshProxy](brls::View*) {
        ProgramConfig::instance().setProxy("");
        refreshProxy();
        return true;
    });
    root->addView(clearProxy);

    // About
    auto* ver = new brls::Label();
    ver->setText(fmt::format("ani-switch {} ({})",
                             APPVersion::instance().getVersionStr(),
                             APPVersion::instance().getPlatform()));
    ver->setFontSize(theme::kTypeCaption);
    ver->setMarginTop(20);
    root->addView(ver);

    // v17.3: re-run onboarding.  Resets the first-run gate so
    // the next startup (or this current push) shows the 5-step
    // welcome again.  Useful for users who skipped it the first
    // time and want to discover features.
    auto* btnOnboard = new brls::Button();
    btnOnboard->setText("重新走引导");
    btnOnboard->setHeight(theme::kButtonHeight);
    theme::applyFocusStyle(btnOnboard);
    btnOnboard->setMarginTop(16);
    btnOnboard->registerClickAction([](brls::View*) {
        ProgramConfig::instance().resetFirstRun();
        brls::Application::popActivity(
            brls::TransitionAnimation::FADE,
            []() { aniswitch::Intent::openOnboarding(); });
        return true;
    });
    root->addView(btnOnboard);

    // v17.4: local video browser.  Scans the SD card for media
    // files and lets the user play them through the existing
    // PlayerActivity.  Pure filesystem, no network.
    auto* btnLocal = new brls::Button();
    btnLocal->setText("本地视频");
    btnLocal->setHeight(theme::kButtonHeight);
    theme::applyFocusStyle(btnLocal);
    btnLocal->setMarginTop(8);
    btnLocal->registerClickAction([](brls::View*) {
        aniswitch::Intent::openLocalVideo();
        return true;
    });
    root->addView(btnLocal);

    // v18.4: Bangumi OAuth sync page (BangumiSyncTab from
    // animeko 6.1.0).  Shows current access/refresh token
    // state and a manual refresh + sign-out button.
    auto* btnSync = new brls::Button();
    btnSync->setText("Bangumi 同步");
    btnSync->setHeight(theme::kButtonHeight);
    theme::applyFocusStyle(btnSync);
    btnSync->setMarginTop(8);
    btnSync->registerClickAction([](brls::View*) {
        aniswitch::Intent::openBangumiSync();
        return true;
    });
    root->addView(btnSync);

    // v18.5: ani server email-OTP login (EmailLoginStartScreen +
    // EmailLoginVerifyScreen from animeko 6.1.0).  Two steps in
    // one activity; an email round-trip then a 6-digit OTP.
    auto* btnEmail = new brls::Button();
    btnEmail->setText("邮箱登录 (ani)");
    btnEmail->setHeight(theme::kButtonHeight);
    theme::applyFocusStyle(btnEmail);
    btnEmail->setMarginTop(8);
    btnEmail->registerClickAction([](brls::View*) {
        aniswitch::Intent::openEmailLoginStart();
        return true;
    });
    root->addView(btnEmail);

    auto* scroll = new brls::ScrollingFrame();
    scroll->setContentView(root);
    setContentView(scroll);
    registerAction("返回", brls::BUTTON_B, [](brls::View*) { brls::Application::popActivity(); return true; });
    SET_TRACE("SET: done");
}

}  // namespace aniswitch
