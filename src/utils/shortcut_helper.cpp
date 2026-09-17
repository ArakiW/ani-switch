// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
#include "utils/shortcut_helper.hpp"
#include <map>
#include <mutex>

namespace aniswitch {

namespace {
    std::map<std::string, std::function<void()>> g_shortcuts;
    std::mutex g_mu;
}  // namespace

void ShortcutHelper::registerShortcut(const std::string& name, std::function<void()> action) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_shortcuts[name] = std::move(action);
}

void ShortcutHelper::runShortcut(const std::string& name) {
    std::function<void()> cb;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        auto it = g_shortcuts.find(name);
        if (it != g_shortcuts.end()) cb = it->second;
    }
    if (cb) cb();
}

}  // namespace aniswitch
