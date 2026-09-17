// SPDX-License-Identifier: AGPL-3.0
//
// On-disk HTML cache for WebSelectorProvider.  Lives in its own
// translation unit so unit tests can link it without pulling in
// borealis / cpr (which `web_selector_provider.cpp` itself needs for
// its live HTTP fetches).
//
// Layout under `<cacheDir>`:
//
//     <cacheDir>/<sanitizedSourceName>/search_<sanitizedKeyword>.html
//     <cacheDir>/<sanitizedSourceName>/detail_<sanitizedKeyword>.html
//
// "Sanitized" replaces any char outside [A-Za-z0-9._-] with '_' and
// caps the result at 64 characters — both source names and keywords
// can be CN/Unicode heavy, and FAT/exFAT SD cards do not appreciate
// surprise bytes in directory entries.

#pragma once

#include <string>

namespace aniswitch::cache {

// Replace every char outside [A-Za-z0-9._-] with '_', cap at 64.
// Exposed so the PC-side prefetch tool can mirror the same encoding
// (and tests can round-trip).
std::string sanitizeForPath(const std::string& in);

// Read a previously-cached search-page HTML.  Returns "" if the cache
// is disabled (empty dir), the file is missing, or the file is empty.
std::string readSearchHtml(const std::string& cacheDir,
                           const std::string& sourceName,
                           const std::string& keyword);

// Read a previously-cached detail-page HTML.  Same semantics as
// readSearchHtml but a different on-disk filename.
std::string readDetailHtml(const std::string& cacheDir,
                           const std::string& sourceName,
                           const std::string& keyword);

}  // namespace aniswitch::cache
