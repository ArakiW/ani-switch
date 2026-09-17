// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
#pragma once

#include <borealis/core/box.hpp>
#include <functional>
#include <string>

namespace aniswitch {

// A simple key-shortcut dispatch helper. The real wiliwili version
// remaps keys to actions; we keep a minimal version that maps A/B/X/Y
// to no-op placeholders so the configuration UI can render and persist
// them.
class ShortcutHelper {
public:
    static void registerShortcut(const std::string& name, std::function<void()> action);
    static void runShortcut(const std::string& name);
};

}  // namespace aniswitch
