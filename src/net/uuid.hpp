// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
//
// Random UUID v4 generator. 36-character canonical form (8-4-4-4-12 hex).

#pragma once

#include <string>

namespace aniswitch {

// Generate a random v4 UUID. Uses std::random_device.
std::string newUuid();

}  // namespace aniswitch
