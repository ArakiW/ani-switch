// SPDX-License-Identifier: AGPL-3.0
//
// User-supplied HTTP(S) URLs keyed by Bangumi episode ID in sources.json.
//
// Key convention (docs/DEVELOPMENT_STATUS.md §9):
//   top-level keys are **Bangumi 集数 ID (episode id)**, e.g. "1227087"
//   for 葬送的芙莉莲 EP1 (subject 400602).  Subject-id keys are also
//   accepted as a fallback so a subject-level entry still resolves.
//
// Soft-fail contract:
//   Missing file / missing key / corrupt JSON / non-HTTP URL entries are
//   NOT hard errors.  enumerate()/lookup() return an empty vector so
//   SourceManager can fall through to web-selector / Bangumi metadata
//   instead of aborting the whole resolve with "解析失败".

#pragma once

#include "core/source_manager.hpp"
#include <string>
#include <vector>

namespace aniswitch {

class HTTPSourceProvider : public ISourceProvider {
public:
    SourceKind kind() const override { return SourceKind::HTTP; }

    // ISourceProvider — look up `episodeId` only. Soft-succeeds empty.
    void enumerate(int32_t episodeId,
                   SourceListCb callback,
                   std::function<void(const std::string&, int)> error) override;

    // Multi-key lookup used by EpisodeResolver.  Keys are tried in
    // order; the first key that yields one or more HTTP(S) entries wins.
    // Returns empty if the file is missing, unparsed, or no key hits.
    static std::vector<VideoSource> lookup(const std::vector<int32_t>& keys);

    // Resolved sources.json path (config dir + "/sources.json").
    static std::string manifestPath();

    // Diagnostics for the resolve UI / logs.
    static bool manifestOk();          // last load parsed as a JSON object
    static size_t manifestKeyCount();  // number of top-level keys (0 if bad)
    static const std::string& manifestPathCached();
    static const std::string& lastError();

    // Test / tool hooks.  Override path (empty clears override) and drop
    // the in-memory cache so the next lookup re-reads from disk.
    static void setManifestPathOverride(const std::string& path);
    static void resetManifestCache();
};

}  // namespace aniswitch
