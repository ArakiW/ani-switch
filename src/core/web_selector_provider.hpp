// SPDX-License-Identifier: AGPL-3.0
//
// WebSelectorProvider — implements ISourceProvider against the animeko
// WebSelector manifest format used by MajoSissi/animeko-source and
// compatible forks (ani-yuan, css, animeko-prime, ...).
//
//   flow:
//     1. At startup, the provider downloads the manifest JSON (default
//        URL points at the gh-proxy mirror of the MajoSissi upstream
//        repo, because raw.githubusercontent.com is rate-limited inside
//        mainland China) and parses it into WebSelectorSource records.
//     2. When asked to enumerate an episode, it looks up a "context"
//        previously pushed by EpisodeResolver (the Bangumi subject name
//        + episode sort) and, for the first K sources, runs the full
//        search-detail-extract chain:
//
//          keyword = "{subjectName} 第 {epNumber} 话"
//          GET searchUrl({keyword})                  <- search page
//          regex <a href=...> subject-name match        <- pick detail URL
//          GET detailUrl                              <- detail page
//          regex matchVideoUrl (m3u8|mp4)             <- playable URL
//
//     3. Sources that don't answer within the timeout, or whose
//        selectors our (very) small HTML parser doesn't understand,
//        silently drop out of the result list.  pickBest() then picks
//        the highest-priority one.
//
// Limitations:
//   - Switch has no JS engine; web-selector manifests that require
//     JavaScript-rendered search/detail pages will not return results.
//   - Our HTML parser handles only the very common `div.x a` /
//     `a[href=...]` selector shapes.  Anything more elaborate
//     (nested `:nth-child`, attribute operators other than `[attr=val]`,
//     pseudo-classes) is skipped.
//   - Subject-name matching is done with std::string::find on the
//     decoded page; if two anime share the same name we pick the first
//     link in the result page.

#pragma once

#include "core/source_manager.hpp"
#include "core/web_selector_data.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace aniswitch {

class WebSelectorProvider : public ISourceProvider {
public:
    WebSelectorProvider();

    SourceKind kind() const override { return SourceKind::HTTP; }

    // Push the metadata we need to actually look up an episode's URL.
    // EpisodeResolver is the only caller; we keep a tiny thread-safe
    // map indexed by episodeId so the value is available to
    // enumerate() whenever the player eventually asks for it.
    void setContext(int32_t episodeId,
                    const std::string& subjectNameCN,
                    const std::string& subjectName,
                    double epSort,
                    const std::string& episodeName);

    // The standard ISourceProvider entry point.  Internally chains
    // off the per-episode context (see setContext).  Sources we
    // could not resolve are silently skipped; if every source fails
    // we call back with an empty vector (which the manager treats as
    // "no sources here, not an error").
    void enumerate(int32_t episodeId,
                   SourceListCb callback,
                   std::function<void(const std::string&, int)> error) override;

    // Force a re-fetch of the upstream manifest (e.g. from a settings
    // UI button).  Public so tests can poke it too.
    void refreshManifest();

    // Override the manifest URL (default: gh-proxy mirror of
    // MajoSissi/animeko-source).
    void setManifestUrl(const std::string& url) { manifestUrl_ = url; }

    // Override the search-page cache root.  When set, before firing
    // an HTTP GET against a source's searchUrl we first try to read
    // `<root>/<sanitized_source_name>/search_<keyword>.html` from
    // disk.  A cache hit lets the source work on the Switch even
    // when the upstream page is JS-rendered (the host browser has
    // already produced the HTML).  The default is the SD card cache
    // path; set to "" to disable.
    void setSearchCacheDir(const std::string& dir) { searchCacheDir_ = dir; }

private:
    struct Ctx {
        std::string subjectNameCN;
        std::string subjectName;
        double      epSort = 0;
        std::string episodeName;
    };

    // Background fetch on first use.  `manifestReady_` flips to true
    // when at least one download attempt has completed (success or
    // failure), so we never block enumerate() forever on a slow net.
    void ensureManifestLoaded();

    // Pull the next playable URL out of a detail page body using
    // `src.matchVideoUrl`.  Returns empty string if no match.
    std::string extractMediaUrl(const std::string& detailHtml,
                                 const WebSelectorSource& src) const;

    // Pick the first <a href="..."> on the search-results page whose
    // visible text contains the subject name.  Returns the absolute
    // URL.  Implements only the "div.x a" / "div.x > a" subset of
    // CSS selectors because that's what most animeko-source entries
    // use.
    std::string pickSubjectLink(const std::string& searchHtml,
                                 const std::string& subjectName,
                                 const WebSelectorSource& src) const;

    // Resolve relative URL against a base.
    static std::string absolutize(const std::string& base, const std::string& ref);

    // ---------------- state ----------------
    std::string manifestUrl_;
    std::string searchCacheDir_ = "sdmc:/switch/aniswitch/webcache";
    std::mutex  manifestMutex_;
    WebSelectorData data_;
    std::atomic<bool> manifestLoading_{false};
    std::atomic<bool> manifestReady_{false};

    std::mutex                ctxMutex_;
    std::unordered_map<int32_t, Ctx> contexts_;

public:
    // Read a previously-cached search HTML for the given source/keyword
    // from disk, or "" if no cache entry exists.  Public for unit tests
    // and for the optional PC-side prefetch tool to round-trip paths.
    std::string readSearchCache(const std::string& sourceName,
                                 const std::string& keyword) const;

    // Same as readSearchCache but for the detail (a.k.a. play) page.
    // Most animeko-source upstreams are JS-rendered SPAs; without a
    // PC-prefetched detail HTML the Switch's HTTP GET returns an
    // empty shell and we never get to the m3u8 extract step.
    std::string readDetailCache(const std::string& sourceName,
                                 const std::string& keyword) const;
};

}  // namespace aniswitch
