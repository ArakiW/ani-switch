// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
//
// Compatibility shim — the wiliwili code calls aniswitch::Register::
// initCustomView() etc. We forward those to the wiliwili-named names
// (for the parts we kept) and to our own initializers for new bits.

#pragma once

#include "utils/config_helper.hpp"

namespace aniswitch {
// Register is defined in config_helper.{hpp,cpp}. This header just makes it
// visible to UI translation units that include it.
}
