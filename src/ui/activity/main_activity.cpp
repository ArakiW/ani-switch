// SPDX-License-Identifier: AGPL-3.0
#include "ui/activity/main_activity.hpp"
#include "ui/fragment/home_rank.hpp"
#include "ui/fragment/home_bangumi.hpp"
#include "ui/fragment/home_recommend.hpp"
#include "ui/hud.hpp"
#include "ui/theme.hpp"
#include "net/http.hpp"
#include "utils/activity_helper.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <borealis/views/scrolling_frame.hpp>
#include <fmt/format.h>
#include <fstream>
#include <string>

#if defined(__SWITCH__)
extern "C" void aniswitchStartupLog(const char* message);
#endif

namespace aniswitch {

MainActivity::MainActivity() = default;
MainActivity::~MainActivity() = default;

void MainActivity::onContentAvailable() {
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
#ifdef __SWITCH__
    root->setWidth(theme::kDesignWidth);
#endif
    root->setBackgroundColor(theme::kChromeBg);

    // ---- Header: brand + secondary actions (left rail removed) ----
    auto* top = new brls::Box();
    top->setAxis(brls::Axis::ROW);
    top->setHeight(theme::kHeaderHeight);
    top->setPadding(12, theme::kSafeMarginX, 12, theme::kSafeMarginX);
    top->setAlignItems(brls::AlignItems::CENTER);
    top->setBackgroundColor(theme::kChromePanel);

    auto* brand = new brls::Label();
    brand->setText("ani");
    brand->setFontSize(theme::kTypeH2);
    brand->setTextColor(theme::kAccentBright);
    brand->setMarginRight(4);
    top->addView(brand);
    auto* brand2 = new brls::Label();
    brand2->setText("-switch");
    brand2->setFontSize(theme::kTypeH2);
    top->addView(brand2);

    auto* spacer = new brls::Box();
    spacer->setGrow(1.0f);
    top->addView(spacer);

    auto addTop = [top](const std::string& text, std::function<void()> action) {
        auto* button = new brls::Button();
        button->setText(text);
        button->setMarginLeft(12);
        button->setHeight(48);
        theme::applyFocusStyle(button, 8.0f);
        button->registerClickAction([action, text](brls::View*) {
#if defined(__SWITCH__)
            std::string m = "MAIN: open " + text;
            aniswitchStartupLog(m.c_str());
#endif
            try {
                action();
            } catch (const std::exception& e) {
                brls::Logger::error("open {} threw: {}", text, e.what());
                brls::Application::notify("打开失败: " + text);
            } catch (...) {
                brls::Application::notify("打开失败: " + text);
            }
            return true;
        });
        top->addView(button);
    };
    addTop("搜索", [] { Intent::openSearch(); });
    addTop("收藏", [] { Intent::openMyCollection(); });
    addTop("历史", [] { Intent::openHistory(); });
    addTop("设置", [] { Intent::openSettings(); });
    root->addView(top);

    // ---- Tab bar ----
    auto* tabBar = new brls::Box();
    tabBar->setAxis(brls::Axis::ROW);
    tabBar->setHeight(theme::kTabHeight);
    tabBar->setPadding(0, theme::kSafeMarginX, 0, theme::kSafeMarginX);
    tabBar->setAlignItems(brls::AlignItems::CENTER);
    root->addView(tabBar);

    content_ = new brls::Box();
    content_->setAxis(brls::Axis::COLUMN);
    content_->setGrow(1.0f);
    content_->setPadding(8, theme::kSafeMarginX, 8, theme::kSafeMarginX);
#ifdef __SWITCH__
    content_->setWidth(theme::kDesignWidth);
#endif
    root->addView(content_);

    tabBtns_.clear();
    tabMakers_.clear();
    activeTab_ = 0;

    auto applyTabStyle = [this]() {
        for (size_t i = 0; i < tabBtns_.size(); ++i) {
            if (!tabBtns_[i]) continue;
            tabBtns_[i]->setStyle(i == activeTab_ ? &brls::BUTTONSTYLE_HIGHLIGHT
                                                  : &brls::BUTTONSTYLE_DEFAULT);
        }
    };
    auto showTab = [this, applyTabStyle](size_t idx) {
        if (idx >= tabMakers_.size() || !content_) return;
        activeTab_ = idx;
        applyTabStyle();
        content_->clearViews();
        auto* next = tabMakers_[idx]();
        next->setGrow(1.0f);
        content_->addView(next);
    };

    auto addTab = [&](const std::string& label, std::function<brls::View*()> make,
                      bool selected) {
        auto* btn = new brls::Button();
        btn->setText(label);
        btn->setMarginRight(8);
        btn->setHeight(48);
        btn->setPadding(0, 20, 0, 20);
        theme::applyFocusStyle(btn, 8.0f);
        const size_t idx = tabBtns_.size();
        btn->registerClickAction([showTab, idx](brls::View*) {
            showTab(idx);
            return true;
        });
        tabBar->addView(btn);
        tabBtns_.push_back(btn);
        tabMakers_.push_back(std::move(make));
        if (selected) showTab(idx);
    };
    addTab("探索", [] { return new HomeRankFragment(); }, true);
    addTab("每日放送", [] { return new HomeBangumiFragment(); }, false);
    addTab("推荐", [] { return new HomeRecommendFragment(); }, false);
    applyTabStyle();

    setContentView(root);

    registerAction("返回", brls::BUTTON_B, [](brls::View*) {
        if (brls::Application::getActivitiesStack().size() > 1) {
            brls::Application::popActivity();
            return true;
        }
        auto* dlg = new brls::Dialog("要退出 ani-switch 吗？");
        dlg->setCancelable(true);
        dlg->addButton("取消", [] {});
        dlg->addButton("退出", [] { brls::Application::quit(); });
        dlg->open();
        return true;
    });

    // L / R cycle home tabs (reuses showTab via makers)
    auto cycle = [this](int dir) {
        if (tabMakers_.empty()) return;
        const int n = static_cast<int>(tabMakers_.size());
        const int next = (static_cast<int>(activeTab_) + dir + n) % n;
        activeTab_ = static_cast<size_t>(next);
        for (size_t i = 0; i < tabBtns_.size(); ++i) {
            if (tabBtns_[i])
                tabBtns_[i]->setStyle(i == activeTab_ ? &brls::BUTTONSTYLE_HIGHLIGHT
                                                      : &brls::BUTTONSTYLE_DEFAULT);
        }
        if (!content_) return;
        content_->clearViews();
        auto* view = tabMakers_[activeTab_]();
        view->setGrow(1.0f);
        content_->addView(view);
    };
    registerAction("上一栏", brls::BUTTON_LB, [cycle](brls::View*) {
        cycle(-1);
        return true;
    });
    registerAction("下一栏", brls::BUTTON_RB, [cycle](brls::View*) {
        cycle(1);
        return true;
    });

    // ---- Bottom HUD (must be built AFTER registerAction so chips
    // reflect the live ActionMap — DESIGN.md §6.4) ----
    root->addView(buildHudFromActions(this->getContentView()));

#if defined(__SWITCH__)
    // v22 field-test: if sdmc:/switch/aniswitch/autotour exists, open
    // that screen after a short delay so emulator/device smoke can
    // screenshot secondary pages without input injection.
    {
        std::ifstream f("sdmc:/switch/aniswitch/autotour");
        std::string mode;
        if (f.good() && std::getline(f, mode)) {
            while (!mode.empty() && (mode.back() == '\r' || mode.back() == '\n'))
                mode.pop_back();
            aniswitchStartupLog("AUTOTOUR: armed");
            // Optional second line for picker/online: "<epId> <subjectId>"
            std::string line2;
            std::getline(f, line2);
            brls::delay(3500, [mode, line2]() {
                char buf[120];
                snprintf(buf, sizeof(buf), "AUTOTOUR: open %s", mode.c_str());
                aniswitchStartupLog(buf);
                auto parseIds = [&line2](int32_t& ep, int32_t& sid) {
                    int e = 0, s = 0;
                    if (sscanf(line2.c_str(), "%d %d", &e, &s) >= 1 && e > 0) {
                        ep = e;
                        if (s > 0) sid = s;
                    }
                };
                if (mode == "collection") Intent::openMyCollection();
                else if (mode == "history") Intent::openHistory();
                else if (mode == "settings") Intent::openSettings();
                else if (mode == "subject") Intent::openSubject(-1);
                else if (mode == "online" || mode == "picker") {
                    int32_t ep = 1227087, sid = 400602;
                    parseIds(ep, sid);
                    {
                        char b2[100];
                        snprintf(b2, sizeof(b2), "AUTOTOUR: picker ep=%d sid=%d", ep, sid);
                        aniswitchStartupLog(b2);
                    }
                    Intent::openSourcePicker(ep, sid, "批量测试源", true);
                } else if (mode == "player") {
                    Intent::openPlayer(-1, "",
                                       "sdmc:/switch/aniswitch/videos/test-local.mp4",
                                       0);
                } else if (mode == "search") {
                    Intent::openSearch();
                } else {
                    Intent::openSearch();
                }
            });
        }
    }
#endif
}

}  // namespace aniswitch
