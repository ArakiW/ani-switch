// SPDX-License-Identifier: AGPL-3.0

#include "ui/activity/log_activity.hpp"
#include "ui/theme.hpp"
#include <borealis/core/application.hpp>
#include <borealis/views/dialog.hpp>
#include <fmt/format.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace aniswitch {

namespace {

// Two well-known locations: the in-process cwd (when libnx
// lands the NRO in place) and the deploy directory on the SD
// card.  We try both so the user can read the log whichever
// way the loader set things up.
std::string resolveLogPath() {
    if (std::filesystem::exists("sdmc:/switch/aniswitch/startup.log")) {
        return "sdmc:/switch/aniswitch/startup.log";
    }
    if (std::filesystem::exists("startup.log")) {
        return "startup.log";
    }
    return "sdmc:/switch/aniswitch/startup.log";  // best-effort default
}

std::string readFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        return std::string("(unable to open ") + path + ")";
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

}  // namespace

LogActivity::LogActivity() = default;

void LogActivity::onContentAvailable() {
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setPadding(20, 20, 20, 20);

    auto* title = new brls::Label();
    title->setText("启动日志");
    title->setFontSize(24);
    title->setMarginBottom(4);
    root->addView(title);

    statusLabel_ = new brls::Label();
    statusLabel_->setFontSize(theme::kTypeCaption);
    statusLabel_->setTextColor(aniswitch::theme::kDarkTextSecondary);
    statusLabel_->setMarginBottom(8);
    root->addView(statusLabel_);

    auto* btnRow = new brls::Box();
    btnRow->setAxis(brls::Axis::ROW);
    btnRow->setMarginBottom(12);

    auto* reloadBtn = new brls::Button();
    reloadBtn->setText("重新读取");
    reloadBtn->registerClickAction([this](brls::View*) {
        reload();
        return true;
    });
    btnRow->addView(reloadBtn);

    auto* spacer = new brls::Box();
    spacer->setWidth(16);
    btnRow->addView(spacer);

    auto* clearBtn = new brls::Button();
    clearBtn->setText("清空日志");
    clearBtn->registerClickAction([](brls::View*) {
        auto* dlg = new brls::Dialog("确定要清空 startup.log?  崩溃时清空就再也看不到堆栈了。");
        dlg->setCancelable(true);
        dlg->addButton("取消", []() {});
        dlg->addButton("清空", []() {
            const std::string path = resolveLogPath();
            FILE* fp = std::fopen(path.c_str(), "w");
            if (fp) {
                std::fclose(fp);
            }
        });
        dlg->open();
        return true;
    });
    btnRow->addView(clearBtn);

    root->addView(btnRow);

    // The log itself goes inside a ScrollingFrame so a 1000-line
    // log doesn't push the buttons off-screen.
    logContainer_ = new brls::Box();
    logContainer_->setAxis(brls::Axis::COLUMN);
    logContainer_->setPadding(0, 0, 0, 0);
    auto* scroll = new brls::ScrollingFrame();
    scroll->setContentView(logContainer_);
    root->addView(scroll);

    setContentView(root);
    registerAction("返回", brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });

    reload();
}

void LogActivity::reload() {
    if (!logContainer_) return;
    while (!logContainer_->getChildren().empty()) {
        logContainer_->removeView(logContainer_->getChildren().front());
    }
    const std::string path = resolveLogPath();
    std::string content = readFile(path);
    if (content.empty()) {
        content = "(空 — startup.log 还没生成, 或 ANISWITCH_SWITCH_DEBUG 没开)";
    }
    statusLabel_->setText(fmt::format("路径: {}  ({} 字节)",
                                       path, content.size()));
    auto* log = new brls::Label();
    log->setText(content);
    log->setFontSize(theme::kTypeCaption);
    log->setSingleLine(false);
    logContainer_->addView(log);
}

}  // namespace aniswitch
