// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
//
// RFC 1321 MD5 implementation. Used by dandanplay's file-matching
// fallback path. 32-character lowercase hex output.

#pragma once

#include <cstdint>
#include <string>
#include <cstring>

namespace aniswitch {

std::string md5(const std::string& input);
std::string md5(const uint8_t* data, size_t len);

}  // namespace aniswitch
