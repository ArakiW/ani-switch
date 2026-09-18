// SPDX-License-Identifier: AGPL-3.0

#include "ui/activity/log_activity.hpp"
#include "ui/theme.hpp"
#include "ui/ui_chrome.hpp"
#include "ui/hud.hpp"
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
    auto* root = chrome::makePageRoot();

    root->addView(chrome::makeTitle("启动日志", theme::kTypeH1, 4));

    statusLabel_ = new brls::Label();
    statusLabel_->setFontSize(theme::kTypeCaption);
    statusLabel_->setTextColor(theme::kDarkTextSecondary);
    statusLabel_->setSingleLine(false);
    statusLabel_->setMarginBottom(12);
    root->addView(statusLabel_);

    root->addView(chrome::makeSection("操作", 8));

    // Primary = reload (safe, frequent). Clear is secondary (destructive).
    root->addView(chrome::makePrimaryButton(
        "重新读取",
        [this]() {
            reload();
        },
        8));

    root->addView(chrome::makeSecondaryButton(
        "清空日志",
        []() {
            auto* dlg = new brls::Dialog(
                "确定要清空 startup.log?  崩溃时清空就再也看不到堆栈了。");
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
        },
        12));

    root->addView(chrome::makeSection("日志内容", 8));

    // Log body lives in this column; the page chrome scroll shell
    // wraps the whole page (title + buttons + log) so long logs
    // remain readable without a nested frame.
    logContainer_ = new brls::Box();
    logContainer_->setAxis(brls::Axis::COLUMN);
    logContainer_->setPadding(0, 0, 0, 0);
    root->addView(logContainer_);

    auto* shell = chrome::attachScrollShell(this, root);
    chrome::registerBack(this);
    chrome::finish(this, shell);

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
    log->setTextColor(theme::kDarkTextSecondary);
    log->setSingleLine(false);
    logContainer_->addView(log);
}

}  // namespace aniswitch
