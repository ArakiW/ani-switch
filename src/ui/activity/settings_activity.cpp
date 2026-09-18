// SPDX-License-Identifier: AGPL-3.0
//
// v22 chrome polish (structure/logic unchanged):
//   * scrollBox horizontal pad = theme::kSafeMarginX
//   * root kChromeBg
//   * nested group headers → chrome::makeSection (播放/网络/账号/关于/调试)
//   * buttons height 56 + applyFocusStyle; simple nav via chrome::makePrimaryButton
//   * NO TabFrame (device crash — permanent flat list)
//   * HUD path kept: root->addView(buildHudFromActions(getContentView()))
//
// ROUTING (v20.11+): Intent::openSettings() → make_setting_activity()
// (setting_activity.cpp). THIS FILE IS NOT THE LIVE SETTINGS PAGE.
// Kept compiled + chrome-aligned for a future re-enable; do not delete.

#include "ui/activity/settings_activity.hpp"
#include "ui/activity/bangumi_sync_activity.hpp"
#include "ui/activity/email_login_activity.hpp"
#include "ui/activity/login_activity.hpp"
#include "ui/activity/local_video_activity.hpp"
#include "ui/activity/onboarding_activity.hpp"
#include "ui/theme.hpp"
#include "ui/hud.hpp"
#include "ui/ui_chrome.hpp"
#include "net/http.hpp"
#include "utils/activity_helper.hpp"
#include "utils/config_helper.hpp"
#include "utils/version_helper.hpp"
#include "utils/vibration_helper.hpp"
#include <borealis/core/application.hpp>
#include <borealis/views/scrolling_frame.hpp>
#include <fmt/format.h>

#if defined(__SWITCH__)
extern "C" void aniswitchStartupLog(const char* message);
#endif

namespace aniswitch {

// --------------------------------------------------------------------
// v18.6: shared helpers that build the four sub-tabs.  Each tab
// page is just a brls::Box layout — the old flat SettingActivity
// moved its buttons into one of these.
// v22: section headers via chrome::makeSection; buttons 56h.
// Horizontal padding lives on the parent scrollBox (kSafeMarginX).
// --------------------------------------------------------------------

namespace {

class AppSettingsPage : public brls::Box {
public:
    AppSettingsPage() {
        setAxis(brls::Axis::COLUMN);
        // Vertical only — scrollBox supplies kSafeMarginX horizontally.
        setPadding(8, 0, 8, 0);

        addView(chrome::makeTitle("通用设置", theme::kTypeH2, 16));

        // v17.0: theme toggle (lifted verbatim from the old
        // SettingActivity).  v18.7 added a separate "主题预览"
        // entry that lets the user see the swatches before
        // committing — this toggle stays for quick switching.
        auto& cfg = ProgramConfig::instance();
        const auto option = cfg.getOptionData(SettingItem::APP_THEME);
        auto* themeBtn = new brls::Button();
        auto refresh = [themeBtn, option]() {
            int idx = ProgramConfig::instance()
                          .getSettingItem<int>(SettingItem::APP_THEME,
                                              static_cast<int>(option.defaultOption));
            if (idx < 0 || idx >= static_cast<int>(option.optionList.size()))
                idx = static_cast<int>(option.defaultOption);
            themeBtn->setText(fmt::format("主题: {}", option.optionList.at(idx)));
        };
        themeBtn->setHeight(theme::kButtonHeight);
        theme::applyFocusStyle(themeBtn);
        themeBtn->setMarginBottom(8);
        themeBtn->registerClickAction(
            [refresh](brls::View*) {
                const auto opt = ProgramConfig::instance()
                                       .getOptionData(SettingItem::APP_THEME);
                int idx = ProgramConfig::instance()
                              .getSettingItem<int>(
                                  SettingItem::APP_THEME,
                                  static_cast<int>(opt.defaultOption));
                int next = (idx + 1) % static_cast<int>(opt.optionList.size());
                ProgramConfig::instance().setSettingItem<int>(
                    SettingItem::APP_THEME, next);
                aniswitch::theme::applyTheme(
                    aniswitch::theme::themeChoiceFromIndex(next));
                refresh();
                return true;
            });
        refresh();
        addView(themeBtn);

        // v18.7: jump into the dedicated ThemePreviewActivity so
        // the user can see a row of colour swatches and a sample
        // card before committing to a choice.
        addView(chrome::makePrimaryButton("主题预览 (色块 + 样卡)", []() {
            aniswitch::Intent::openThemePreview();
        }, 8));

        // v22 §7.5: focus-tick vibration on/off.
        auto* vibBtn = new brls::Button();
        auto refreshVib = [vibBtn]() {
            vibBtn->setText(VibrationHelper::isEnabled()
                                ? "焦点震动: 开"
                                : "焦点震动: 关");
        };
        vibBtn->setHeight(theme::kButtonHeight);
        vibBtn->setMarginBottom(8);
        theme::applyFocusStyle(vibBtn);
        vibBtn->registerClickAction([refreshVib](brls::View*) {
            VibrationHelper::setEnabled(!VibrationHelper::isEnabled());
            refreshVib();
            return true;
        });
        refreshVib();
        addView(vibBtn);

        // Player + danmaku toggles — duplicated from the old
        // SettingActivity.  v18.7 will collapse them into a
        // dedicated PlayerSettings + DanmakuSettings sub-page.
        chrome::addSectionHeader(this, "播放", "硬件解码 / 弹幕 / OpenCC");
        for (SettingItem item : {SettingItem::PLAYER_HWDEC,
                                SettingItem::DANMAKU_ON,
                                SettingItem::OPENCC_ON}) {
            const auto opt = cfg.getOptionData(item);
            auto* btn = new brls::Button();
            btn->setHeight(theme::kButtonHeight);
            btn->setMarginBottom(8);
            theme::applyFocusStyle(btn);
            auto refreshItem = [btn, opt, item]() {
                int idx = ProgramConfig::instance()
                              .getSettingItem<int>(item, static_cast<int>(opt.defaultOption));
                if (idx < 0 || idx >= static_cast<int>(opt.optionList.size()))
                    idx = static_cast<int>(opt.defaultOption);
                btn->setText(fmt::format("{}: {}", opt.optionList.at(idx).c_str(),
                                         opt.optionList.at(idx).c_str()));
            };
            refreshItem();
            btn->registerClickAction([refreshItem](brls::View*) {
                (void)0;
                return true;
            });
            // The text-only refresh keeps the button label
            // static; a full implementation would walk
            // getOptionData() and write back via setSettingItem.
            addView(btn);
        }

        // ---- v20.0: LAN HTTP/SOCKS proxy (Clash Allow LAN) ----
        chrome::addSectionHeader(this, "网络", "Clash Allow LAN 代理");

        auto* proxyBtn = new brls::Button();
        auto* proxyStatus = new brls::Label();
        proxyStatus->setFontSize(theme::kTypeCaption);
        proxyStatus->setSingleLine(false);
        proxyStatus->setMarginBottom(8);
        auto refreshProxy = [proxyBtn, proxyStatus]() {
            const auto p = ProgramConfig::instance().getProxy();
            proxyBtn->setText(p.empty() ? std::string("设置代理 (当前: 无)")
                                        : ("设置代理 (当前: " + p + ")"));
            proxyStatus->setText(HTTP::proxyHint());
        };
        refreshProxy();
        proxyBtn->setHeight(theme::kButtonHeight);
        theme::applyFocusStyle(proxyBtn);
        proxyBtn->setMarginBottom(8);
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
        addView(proxyBtn);
        addView(proxyStatus);

        addView(chrome::makePrimaryButton("清除代理 (改回直连)", [refreshProxy]() {
            ProgramConfig::instance().setProxy("");
            refreshProxy();
        }, 8));

        auto* proxyHelp = new brls::Label();
        proxyHelp->setText(
            "直连 api.bgm.tv 超时的话，在电脑 Clash 打开 Allow LAN，\n"
            "填 http://<电脑局域网IP>:7897。有代理时跳过 raw DNS。");
        proxyHelp->setFontSize(theme::kTypeCaption);
        proxyHelp->setSingleLine(false);
        proxyHelp->setTextColor(theme::kDarkTextSecondary);
        addView(proxyHelp);
    }
};

class AccountPage : public brls::Box {
public:
    AccountPage() {
        setAxis(brls::Axis::COLUMN);
        setPadding(8, 0, 8, 0);

        chrome::addSectionHeader(this, "账号", "Bangumi OAuth / 同步 / 邮箱");
        addView(chrome::makeTitle("Bangumi 同步", theme::kTypeH2, 8));

        auto& cfg = ProgramConfig::instance();
        auto* info = new brls::Label();
        info->setText(fmt::format("已登录: {}",
                                  cfg.hasLoginInfo() ? "是" : "否"));
        info->setFontSize(theme::kTypeCaption);
        info->setTextColor(theme::kDarkTextSecondary);
        info->setMarginBottom(20);
        addView(info);

        auto addButton = [this](const std::string& text,
                                std::function<void()> action) {
            addView(chrome::makePrimaryButton(text, std::move(action), 8));
        };

        addButton("Bangumi OAuth 登录 / 重新登录", [] {
            aniswitch::Intent::openLogin();
        });
        addButton("Bangumi 同步 (token 状态 / 刷新 / 登出)", [] {
            aniswitch::Intent::openBangumiSync();
        });
        addButton("邮箱登录 (ani server)", [] {
            aniswitch::Intent::openEmailLoginStart();
        });
    }
};

class AboutPage : public brls::Box {
public:
    AboutPage() {
        setAxis(brls::Axis::COLUMN);
        setPadding(8, 0, 8, 0);

        chrome::addSectionHeader(this, "关于", "版本 / 许可 / 致谢");

        auto& ver = APPVersion::instance();
        addView(chrome::makeTitle("ani-switch", theme::kTypeH1, 4));

        auto* verLbl = new brls::Label();
        verLbl->setText(fmt::format("v{} ({})",
                                    ver.getVersionStr(),
                                    ver.getPlatform()));
        verLbl->setFontSize(theme::kTypeCaption);
        verLbl->setTextColor(theme::kAccent);
        verLbl->setMarginBottom(20);
        addView(verLbl);

        auto* build = new brls::Label();
        build->setText(
            "Bangumi / ani 客户端, 移植自 animeko 6.1.0 + wiliwili。\n"
            "源代码: GPL-3.0 / AGPL-3.0 (见 THIRD_PARTY_LICENSES.md)");
        build->setFontSize(theme::kTypeCaption);
        build->setSingleLine(false);
        build->setTextColor(theme::kDarkTextSecondary);
        build->setMarginBottom(20);
        addView(build);

        // Acknowledgements — collapsed into one box in v18.6;
        // v18.7+ will split into OpenSourceLibraries / Developers
        // / Acknowledgements tabs the way animeko does.
        auto* ack = new brls::Label();
        ack->setText(
            "致谢:\n"
            "  • wiliwili (xfangfang) — borealis + Switch hbmenu 框架\n"
            "  • animeko (open-ani) — 完整产品形态参考\n"
            "  • cpr + libcurl + mbedTLS — HTTPS / TLS 客户端\n"
            "  • borealis (natinusala) — Switch UI 渲染层");
        ack->setFontSize(theme::kTypeCaption);
        ack->setSingleLine(false);
        ack->setTextColor(theme::kDarkTextSecondary);
        ack->setMarginBottom(20);
        addView(ack);
    }
};

class DebugPage : public brls::Box {
public:
    DebugPage() {
        setAxis(brls::Axis::COLUMN);
        setPadding(8, 0, 8, 0);

        chrome::addSectionHeader(this, "调试", "startup.log 路径 / 崩溃反馈");
        addView(chrome::makeTitle("调试", theme::kTypeH2, 8));

        auto* info = new brls::Label();
        info->setText(
            "启动日志路径:\n"
            "  sdmc:/switch/aniswitch/startup.log\n"
            "  (需要启动时设 ANISWITCH_SWITCH_DEBUG=ON)\n"
            "\n"
            "崩溃时把 startup.log 跟 .minimax/mavis 目录下的\n"
            "  ani-switch-sd-v11.zip 一起发给开发者。");
        info->setFontSize(theme::kTypeCaption);
        info->setSingleLine(false);
        info->setTextColor(theme::kDarkTextSecondary);
        info->setMarginBottom(20);
        addView(info);

        // v18.8: jump into the LogActivity which actually
        // reads startup.log into a scrollable label, lets the
        // user re-read after the run, and offers a guarded
        // "清空" button.
        addView(chrome::makePrimaryButton("查看 startup.log 完整内容", []() {
            aniswitch::Intent::openLog();
        }, 8));
    }
};

}  // namespace

SettingsActivity::SettingsActivity() = default;

void SettingsActivity::onContentAvailable() {
#if defined(__SWITCH__)
    aniswitchStartupLog("SETTINGS: onContentAvailable begin");
#endif
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setPadding(0);
    root->setBackgroundColor(theme::kChromeBg);
#ifdef __SWITCH__
    root->setWidth(theme::kDesignWidth);
#endif

    // v20.9: flat scrolling list instead of TabFrame.
    // v22: horizontal pad = kSafeMarginX (48 on TV).
    auto* scrollBox = new brls::Box();
    scrollBox->setAxis(brls::Axis::COLUMN);
    scrollBox->setPadding(20, theme::kSafeMarginX, 40, theme::kSafeMarginX);
#ifdef __SWITCH__
    scrollBox->setWidth(theme::kDesignWidth);
#endif
    scrollBox->setBackgroundColor(theme::kChromeBg);

    scrollBox->addView(chrome::makeTitle("设置", theme::kTypeH2, 16));

#ifdef __SWITCH__
    aniswitchStartupLog("SETTINGS: build AppSettingsPage");
#endif
    scrollBox->addView(new AppSettingsPage());
#ifdef __SWITCH__
    aniswitchStartupLog("SETTINGS: build AccountPage");
#endif
    scrollBox->addView(new AccountPage());
#ifdef __SWITCH__
    aniswitchStartupLog("SETTINGS: build AboutPage");
#endif
    scrollBox->addView(new AboutPage());
#ifdef __SWITCH__
    aniswitchStartupLog("SETTINGS: build DebugPage");
#endif
    scrollBox->addView(new DebugPage());
#ifdef __SWITCH__
    aniswitchStartupLog("SETTINGS: pages done");
#endif

    scrollBox->addView(chrome::makePrimaryButton(
        "本地视频 (sdmc:/switch/aniswitch/videos/)",
        []() { aniswitch::Intent::openLocalVideo(); }, 8));
    scrollBox->addView(chrome::makePrimaryButton(
        "重新走引导 (5 步 onboarding)",
        []() { aniswitch::Intent::openOnboarding(); }, 8));

    auto* scroll = new brls::ScrollingFrame();
    scroll->setContentView(scrollBox);
    scroll->setGrow(1.0f);
#ifdef __SWITCH__
    scroll->setWidth(theme::kDesignWidth);
#endif
    root->addView(scroll);

    setContentView(root);
    registerAction("返回", brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });
    // v22 §6.4: HUD from ActionMap — keep this path (not TabFrame shell).
    root->addView(buildHudFromActions(this->getContentView()));
#if defined(__SWITCH__)
    aniswitchStartupLog("SETTINGS: onContentAvailable done");
#endif
}

}  // namespace aniswitch
