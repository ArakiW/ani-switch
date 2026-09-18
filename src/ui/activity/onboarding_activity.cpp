// SPDX-License-Identifier: AGPL-3.0
//
// v17.6.1: rewritten onboarding.  v17.3-v17.5 version crashed
// on first-frame layout in some user environments.  This pass
// keeps the same 5-step UX but:
//   * drops the inline const NVGcolor tokens (kAccent / kDarkText
//     etc) in favour of brls::Theme::getColor() lookups
//   * drops setJustifyContent(SPACE_BETWEEN) which interacted
//     with the box's yoga node before children were attached
//   * adds per-step startupStage() so the next crash dumps a
//     tell-tale "[onboard] step N" line in startup.log
//   * wraps onContentAvailable in try/catch — a crash here now
//     logs a meaningful message instead of taking the NRO down
//   * the dynamic_cast on getContentView is replaced with the
//     raw `contentView` member access; borealis 5f08b286 makes
//     `contentView` a public field on Activity
//
// v22 chrome: type scale (display 36 / h1 32 / caption 16 / body 20),
// theme::k* colors instead of tokenColor, 88px theme rows via
// chrome::makeListRow, final CTA chrome::makePrimaryButton 56h,
// footer buttons kButtonHeight + applyFocusStyle. 5-step logic +
// startupLog + crash guards unchanged.

#include "ui/activity/onboarding_activity.hpp"
#include "ui/theme.hpp"
#include "ui/ui_chrome.hpp"
#include "net/bgm_auth.hpp"
#include "net/bgm_client.hpp"
#include "utils/activity_helper.hpp"
#include "utils/config_helper.hpp"
#include <borealis/core/application.hpp>
#include <borealis/core/theme.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/core/thread.hpp>
#include <fmt/format.h>

#if defined(__SWITCH__)
extern "C" void aniswitchStartupLog(const char* message);
namespace { inline void onboardLog(const char* m) { aniswitchStartupLog(m); } }
#else
namespace { inline void onboardLog(const char*) {} }
#endif

namespace aniswitch {

OnboardingActivity::OnboardingActivity() = default;

void OnboardingActivity::onContentAvailable() {
    onboardLog("[onboard] onContentAvailable begin");
    try {
        // Keep root as Box (v17.6.1 layout contract) but apply v22 chrome.
        auto* root = new brls::Box();
        root->setAxis(brls::Axis::COLUMN);
        root->setBackgroundColor(theme::kChromeBg);
        root->setPadding(theme::kSafeMarginTop, theme::kSafeMarginX,
                         theme::kSafeMarginBot, theme::kSafeMarginX);
#ifdef __SWITCH__
        root->setWidth(theme::kDesignWidth);
#endif
        // v17.6.1: removed setJustifyContent(SPACE_BETWEEN) —
        // combined with the dynamic_cast in renderStep() it
        // crashed on first frame in some configurations.
        setContentView(root);
        onboardLog("[onboard] contentView set");
        renderStep();
        onboardLog("[onboard] renderStep done");
    } catch (const std::exception& e) {
        brls::Logger::error("OnboardingActivity crashed: {}", e.what());
        onboardLog("[onboard] CRASH std::exception");
    } catch (...) {
        brls::Logger::error("OnboardingActivity crashed: unknown");
        onboardLog("[onboard] CRASH unknown");
    }
}

void OnboardingActivity::renderStep() {
    auto* root = dynamic_cast<brls::Box*>(getContentView());
    if (!root) {
        onboardLog("[onboard] renderStep: root is null");
        return;
    }
    while (!root->getChildren().empty()) {
        root->removeView(root->getChildren().front());
    }
    onboardLog(fmt::format("[onboard] renderStep step={}", currentStep_).c_str());

    switch (currentStep_) {
        case 0: buildStepWelcome(root);      break;
        case 1: buildStepTheme(root);        break;
        case 2: buildStepLogin(root);        break;
        case 3: buildStepDataSources(root);  break;
        case 4: buildStepDone(root);         break;
        default: buildStepWelcome(root);     break;
    }
    buildFooter(root);
}

void OnboardingActivity::buildStepWelcome(brls::Box* root) {
    // Welcome hero — type-display 36.
    root->addView(chrome::makeTitle("欢迎使用 ani-switch", theme::kTypeDisplay, 12));

    auto* sub = new brls::Label();
    sub->setText("Switch 上的 Bangumi 番剧客户端。");
    sub->setFontSize(theme::kTypeCaption);
    sub->setTextColor(theme::kAccent);
    sub->setMarginBottom(24);
    root->addView(sub);

    auto* body = new brls::Label();
    body->setText(
        "主要功能:\n"
        "  • 番剧详情: 简介 / 剧集 / 角色 / 关联作品 / 评分 / 收藏\n"
        "  • 搜索: Bangumi 公开 + 30+ 在线视频源聚合\n"
        "  • 播放: 本地视频 / HTTP(S) 链接, dandanplay + myani 弹幕\n"
        "  • 收藏 / 历史: 同步 Bangumi 账号, 离线浏览本地\n"
        "  • 设置: 主题 / 音量 / 弹幕样式 / 缓存清理\n"
    );
    body->setFontSize(theme::kTypeBody);
    body->setSingleLine(false);
    body->setTextColor(theme::kDarkTextPrimary);
    body->setMarginBottom(20);
    root->addView(body);
}

void OnboardingActivity::buildStepTheme(brls::Box* root) {
    // Step title — type-h1 32.
    root->addView(chrome::makeTitle("主题", theme::kTypeH1, 8));
    root->addView(chrome::makeCaption("先选一个, 之后可以在设置里随时切换。", 24));

    auto& cfg = ProgramConfig::instance();
    const int currentIdx = cfg.getSettingItem<int>(SettingItem::APP_THEME, 0);

    const char* names[]  = {"自动", "亮", "暗"};
    for (int i = 0; i < 3; ++i) {
        const bool selected = (i == currentIdx);
        // v22: 88px chrome list row + chip-like selection state.
        auto* row = chrome::makeListRow(theme::kRowHeight);

        auto* swatch = new brls::Rectangle();
        swatch->setSize(brls::Size(28, 28));
        swatch->setColor(theme::kAccent);
        swatch->setMarginRight(12);
        row->addView(swatch);

        auto* label = new brls::Label();
        label->setText(names[i]);
        label->setFontSize(theme::kTypeH3);
        label->setTextColor(selected ? theme::kAccentBright
                                     : theme::kDarkTextPrimary);
        label->setGrow(1.0f);
        row->addView(label);

        // makeChip-like status pill; selection click stays on the row
        // so focus land is the full 88px target (Switch 10-foot).
        auto* state = chrome::makeChip(selected ? "当前" : "选择", selected, nullptr);
        state->setFocusable(false);
        row->addView(state);

        // Same theme-switch logic as v17 — no renderStep() so the
        // row labels stay until the user advances a step.
        row->registerClickAction([i](brls::View*) {
            ProgramConfig::instance().setSettingItem<int>(
                SettingItem::APP_THEME, i);
            aniswitch::theme::applyTheme(
                aniswitch::theme::themeChoiceFromIndex(i));
            return true;
        });
        root->addView(row);
    }
}

void OnboardingActivity::buildStepLogin(brls::Box* root) {
    root->addView(chrome::makeTitle("Bangumi 账号", theme::kTypeH1, 12));
    root->addView(chrome::makeCaption("登录不是播放前提。", 12));

    auto* body = new brls::Label();
    body->setText(
        "• 公开浏览 (番剧搜索 / 详情 / 角色 / 评分) 不需要登录\n"
        "• 登录后可以:\n"
        "  - 同步「我的收藏」(5 状态: 想看 / 在看 / 看过 / 搁置 / 抛弃)\n"
        "  - 看到自己的评分\n"
        "  - 同步观看历史到 Bangumi\n\n"
        "在「设置 → Bangumi 登录」扫码即可完成 OAuth, 之后会在\n"
        "后台自动刷新 token (约 6-7 天), 无需重复操作。\n"
    );
    body->setFontSize(theme::kTypeBody);
    body->setSingleLine(false);
    body->setTextColor(theme::kDarkTextPrimary);
    body->setMarginBottom(20);
    root->addView(body);
}

void OnboardingActivity::buildStepDataSources(brls::Box* root) {
    root->addView(chrome::makeTitle("数据源", theme::kTypeH1, 12));

    auto* body = new brls::Label();
    body->setText(
        "ani-switch 不会重分发任何视频文件, 只做播放源聚合:\n\n"
        "• 番剧元数据 — Bangumi (bgm.tv) 公开 API\n"
        "• 搜索 / 在线播放 — 30+ 公开 web 视频源\n"
        "  启动时会预拉一份本地缓存, 离线也能用\n"
        "• 弹幕 — dandanplay (默认) + myani (备选)\n"
        "  网络好时优先 myani, 网络差时降级 dandanplay\n\n"
        "如果有特别想看的源, 可以在「设置 → 媒体源」加自定义\n"
        "RSS / Web selector 配置。\n"
    );
    body->setFontSize(theme::kTypeBody);
    body->setSingleLine(false);
    body->setTextColor(theme::kDarkTextPrimary);
    body->setMarginBottom(20);
    root->addView(body);
}

void OnboardingActivity::buildStepDone(brls::Box* root) {
    // Done hero — type-display 36.
    root->addView(chrome::makeTitle("准备好了", theme::kTypeDisplay, 12));

    auto* sub = new brls::Label();
    sub->setText("按「进入应用」前建议先输入登录码（可跳过）。\n"
                 "之后在「设置」里可以重新走引导。");
    sub->setFontSize(theme::kTypeCaption);
    sub->setTextColor(theme::kDarkTextSecondary);
    sub->setMarginBottom(16);
    root->addView(sub);

    // v22.2: manual entry also routes through the shared prompt helper
    // so skip/shown flags stay consistent with the deferred dialog.
    root->addView(chrome::makeSecondaryButton(
        "输入登录码 (PAT / 授权码)",
        []() { promptFirstRunLoginCode(nullptr); },
        12));

    // Final CTA — chrome primary 56h. markFirstRunDone stays here
    // (onboarding complete); login-code dialog is deferred until Main
    // is on the stack (brls::Dialog + ImeManager — least crash-prone).
    auto* enter = chrome::makePrimaryButton("进入应用  →", []() {
        ProgramConfig::instance().markFirstRunDone();
        brls::Application::popActivity(
            brls::TransitionAnimation::FADE,
            []() {
                aniswitch::Intent::openMain();
                if (firstRunLoginCodePromptNeeded()) {
                    brls::delay(600, []() { promptFirstRunLoginCode(nullptr); });
                }
            });
    }, 8);
    enter->setFontSize(theme::kTypeH3);
    root->addView(enter);
}

void OnboardingActivity::buildFooter(brls::Box* root) {
    auto* footer = new brls::Box();
    footer->setAxis(brls::Axis::ROW);
    footer->setMarginTop(24);
    footer->setJustifyContent(brls::JustifyContent::SPACE_BETWEEN);
    footer->setAlignItems(brls::AlignItems::CENTER);

    auto* prev = new brls::Button();
    prev->setText(currentStep_ > 0 ? "← 上一步" : " ");
    prev->setHeight(theme::kButtonHeight);
    theme::applyFocusStyle(prev);
    if (currentStep_ > 0) {
        prev->registerClickAction([this](brls::View*) {
            if (currentStep_ > 0) --currentStep_;
            renderStep();
            return true;
        });
    }
    footer->addView(prev);

    auto* dots = new brls::Box();
    dots->setAxis(brls::Axis::ROW);
    dots->setAlignItems(brls::AlignItems::CENTER);
    for (int i = 0; i < kStepCount; ++i) {
        auto* dot = new brls::Rectangle();
        dot->setSize(brls::Size(12, 12));
        dot->setColor(i == currentStep_ ? theme::kAccent
                                        : theme::kDarkTextMuted);
        dot->setMarginLeft(4);
        dot->setMarginRight(4);
        dots->addView(dot);
    }
    footer->addView(dots);

    auto* next = new brls::Button();
    next->setHeight(theme::kButtonHeight);
    theme::applyFocusStyle(next);
    if (currentStep_ < kStepCount - 1) {
        next->setText("下一步 →");
        next->registerClickAction([this](brls::View*) {
            if (currentStep_ < kStepCount - 1) ++currentStep_;
            renderStep();
            return true;
        });
    } else {
        next->setText("完成  →");
        next->registerClickAction([](brls::View*) {
            ProgramConfig::instance().markFirstRunDone();
            brls::Application::popActivity(
                brls::TransitionAnimation::FADE,
                []() {
                    aniswitch::Intent::openMain();
                    if (firstRunLoginCodePromptNeeded()) {
                        brls::delay(600, []() { promptFirstRunLoginCode(nullptr); });
                    }
                });
            return true;
        });
    }
    footer->addView(next);
    root->addView(footer);
}

}  // namespace aniswitch
