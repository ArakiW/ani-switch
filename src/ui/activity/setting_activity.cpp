// SPDX-License-Identifier: AGPL-3.0
//
// ani-switch PATCH (2026-09-05): vendored borealis 5f08b286 doesn't have
// brls::Toggle (it's a newer API). Replace the toggle rows with simple
// Button rows that flip the underlying config item on each click.
//
// v22 chrome: kChromeBg root, kSafeMarginX pad, chrome::makeSection groups
// (播放/网络/账号/关于), buttons 56h + applyFocusStyle / makePrimaryButton,
// scroll shell + HUD + B 返回. All settings logic unchanged.
//
// ROUTING: this is the LIVE settings screen (Intent::openSettings →
// make_setting_activity since v20.11). SettingsActivity is the unused
// multi-page twin kept for a future re-enable.

#include "ui/activity/setting_activity.hpp"
#include "ui/theme.hpp"
#include "ui/ui_chrome.hpp"
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

    auto* content = chrome::makePageRoot();

    // Header — title kTypeH2 + auth caption.
    content->addView(chrome::makeTitle("设置", theme::kTypeH2, 4));
    auto* authLabel = new brls::Label();
    authLabel->setText(cfg.hasLoginInfo()
        ? fmt::format("已登录: user {}", cfg.getUserID())
        : "未登录");
    authLabel->setFontSize(theme::kTypeCaption);
    authLabel->setTextColor(theme::kDarkTextSecondary);
    authLabel->setMarginBottom(16);
    content->addView(authLabel);

    // ---- 播放 ----
    chrome::addSectionHeader(content, "播放", "硬件解码 / 弹幕 / 加载方式 / 主题");
    content->addView(makeToggleButton(SettingItem::PLAYER_HWDEC, "硬件解码", cfg));
    content->addView(makeToggleButton(SettingItem::DANMAKU_ON,   "启用弹幕", cfg));
    // v22.3 experiment: seamless (ani:// local) vs mpv-direct (wiliwili).
    content->addView(makeToggleButton(
        SettingItem::PLAYER_STREAM_MODE, "在线加载(无缝/直连)", cfg));
    content->addView(chrome::makeMuted(
        "seamless=应用下载后单流播放(稳) · mpv-direct=mpv 直连 m3u8(需代理/DNS 可用)",
        8));
    // v17.0: theme switcher.  Sits next to the player toggles so
    // it's easy to find without making a separate "appearance"
    // sub-screen.  applyTheme() inside the click handler does
    // the live re-skin; ProgramConfig::save() persists the
    // choice on disk.
    content->addView(makeThemeButton(cfg));

    // ---- 网络 ----
    chrome::addSectionHeader(content, "网络", "Clash Allow LAN 代理");

    auto* proxyBtn = new brls::Button();
    auto* proxyHint = new brls::Label();
    proxyHint->setFontSize(theme::kTypeCaption);
    proxyHint->setTextColor(theme::kDarkTextSecondary);
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
    content->addView(proxyBtn);
    content->addView(proxyHint);
    content->addView(chrome::makePrimaryButton("清除代理 (改回直连)", [refreshProxy]() {
        ProgramConfig::instance().setProxy("");
        refreshProxy();
    }, 8));

    // ---- 账号 ----
    chrome::addSectionHeader(content, "账号", "Bangumi OAuth / 同步 / 邮箱登录");
    content->addView(chrome::makePrimaryButton(
        cfg.hasLoginInfo() ? "重新登录" : "登录 Bangumi",
        []() { Intent::openLogin(); }, 8));
    content->addView(chrome::makePrimaryButton("Bangumi 同步", []() {
        aniswitch::Intent::openBangumiSync();
    }, 8));
    content->addView(chrome::makePrimaryButton("邮箱登录 (ani)", []() {
        aniswitch::Intent::openEmailLoginStart();
    }, 8));

    // ---- 关于 / 引导 ----
    chrome::addSectionHeader(content, "关于", "版本 / 引导 / 本地视频");
    auto* ver = new brls::Label();
    ver->setText(fmt::format("ani-switch {} ({})",
                             APPVersion::instance().getVersionStr(),
                             APPVersion::instance().getPlatform()));
    ver->setFontSize(theme::kTypeCaption);
    ver->setTextColor(theme::kDarkTextSecondary);
    ver->setMarginBottom(8);
    content->addView(ver);

    // v17.3: re-run onboarding.  Resets the first-run gate so
    // the next startup (or this current push) shows the 5-step
    // welcome again.  Useful for users who skipped it the first
    // time and want to discover features.
    content->addView(chrome::makePrimaryButton("重新走引导", []() {
        ProgramConfig::instance().resetFirstRun();
        brls::Application::popActivity(
            brls::TransitionAnimation::FADE,
            []() { aniswitch::Intent::openOnboarding(); });
    }, 8));

    // v17.4: local video browser.  Scans the SD card for media
    // files and lets the user play them through the existing
    // PlayerActivity.  Pure filesystem, no network.
    content->addView(chrome::makePrimaryButton("本地视频", []() {
        aniswitch::Intent::openLocalVideo();
    }, 8));

    auto* shell = chrome::attachScrollShell(this, content);
    if (shell) shell->setBackgroundColor(theme::kChromeBg);
    chrome::registerBack(this);
    SET_TRACE("SET: hud");
    chrome::finish(this, shell);
    SET_TRACE("SET: done");
}

}  // namespace aniswitch
