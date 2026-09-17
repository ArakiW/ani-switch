// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
#include "net/json_helper.hpp"

namespace aniswitch {

std::string parseLink(const std::string& url) {
    if (url.empty()) return url;
    if (url.find("://") != std::string::npos) return url;  // already absolute
    if (url[0] == '/') return url;                          // site-relative
    return url;
}

}  // namespace aniswitch
