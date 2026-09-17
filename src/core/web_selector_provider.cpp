// SPDX-License-Identifier: AGPL-3.0
#include "core/web_selector_provider.hpp"
#include "core/web_selector_cache.hpp"
#include "net/http.hpp"
#include <borealis/core/logger.hpp>
#include <cpr/cpr.h>
#include <fmt/format.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>
#include <sys/stat.h>

namespace aniswitch {

namespace {

// Trim a copy of `s`.  Whitespace is anything that std::isspace would
// accept; we don't care about locale.
std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

// Crude URL-encode for the bits that animeko-source search URLs
// actually need (the {keyword} placeholder).  We don't try to be RFC
// 3986 perfect 鈥?cpr already does that on the full URL we hand it.
std::string urlEncodeKeyword(const std::string& s) {
    std::string out;
    out.reserve(s.size() * 3);
    static const char* hex = "0123456789ABCDEF";
    for (unsigned char c : s) {
        if (std::isalnum(c) || c=='-' || c=='_' || c=='.' || c=='~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0xF]);
        }
    }
    return out;
}

// Decode HTML entities we see in anime names: the named ones plus
// numeric refs (decimal `&#NNN;` and hex `&#xNN;`).  v15.4 widened
// the named set and added numeric because the prefetch smoke test
// surfaced 33 detail misses that were all from search pages using
// `&#039;` for `'` in subject titles.
std::string decodeEntities(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    auto appendNumeric = [&](size_t start, size_t end, bool hex) -> size_t {
        // start points to the first digit (or 'x' for hex); end
        // points one past the last digit.
        if (start >= end) return 0;
        unsigned int code = 0;
        for (size_t k = start; k < end; ++k) {
            const char c = s[k];
            int digit = -1;
            if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'a' && c <= 'f') digit = 10 + c - 'a';
            else if (c >= 'A' && c <= 'F') digit = 10 + c - 'A';
            if (digit < 0) return 0;
            code = code * (hex ? 16 : 10) + static_cast<unsigned int>(digit);
            if (code > 0x10FFFF) return 0;
        }
        // Render as UTF-8.
        if (code < 0x80) {
            out.push_back(static_cast<char>(code));
        } else if (code < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else if (code < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (code >> 18)));
            out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
        return end - (hex ? 2 : 1);  // consumed up to and including last digit
    };
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '&' && i + 1 < s.size()) {
            if (s.compare(i, 5, "&amp;") == 0)         { out.push_back('&');  i += 5; continue; }
            if (s.compare(i, 4, "&lt;") == 0)          { out.push_back('<');  i += 4; continue; }
            if (s.compare(i, 4, "&gt;") == 0)          { out.push_back('>');  i += 4; continue; }
            if (s.compare(i, 6, "&quot;") == 0)         { out.push_back('"');  i += 6; continue; }
            if (s.compare(i, 5, "&#39;") == 0)          { out.push_back('\''); i += 5; continue; }
            if (s.compare(i, 5, "&apos;") == 0)         { out.push_back('\''); i += 5; continue; }
            if (s.compare(i, 5, "&nbsp;") == 0)         { out.push_back(' ');  i += 6; continue; }
            // Numeric: &#NNN; or &#xNN;
            if (i + 3 < s.size() && s[i + 1] == '#') {
                if (s[i + 2] == 'x' || s[i + 2] == 'X') {
                    size_t end = s.find(';', i + 3);
                    if (end != std::string::npos) {
                        size_t consumed = appendNumeric(i + 3, end, true);
                        if (consumed) { i = end + 1; continue; }
                    }
                } else if (s[i + 2] >= '0' && s[i + 2] <= '9') {
                    size_t end = s.find(';', i + 2);
                    if (end != std::string::npos) {
                        size_t consumed = appendNumeric(i + 2, end, false);
                        if (consumed) { i = end + 1; continue; }
                    }
                }
            }
        }
        out.push_back(s[i++]);
    }
    return out;
}

// Very small CSS selector parser.  Accepts only:
//   - "tag"
//   - "tag#id"        (one id only)
//   - "tag.cls.cls"   (one or more classes)
//   - "ancestor desc"  (descendant combinator)
//
// We only use it to verify that a <tag> we already located by raw
// regex (see findAnchor) matches the configured selector.  If the
// selector is anything we don't understand we return true (be liberal)
// so we don't accidentally drop a working source because of a
// parser gap.
bool cssSelectorAccepts(const std::string& sel, const std::string& tag,
                        const std::string& id, const std::string& classes) {
    auto matchOne = [&](const std::string& one) -> bool {
        if (one.empty()) return true;
        // tag
        size_t i = 0;
        if (std::isalpha(static_cast<unsigned char>(one[i]))) {
            std::string t;
            while (i < one.size() && (std::isalnum(static_cast<unsigned char>(one[i])) || one[i] == '-')) {
                t.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(one[i]))));
                ++i;
            }
            std::string ltag = tag;
            std::transform(ltag.begin(), ltag.end(), ltag.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            if (ltag != t) return false;
        }
        while (i < one.size()) {
            if (one[i] == '#') {
                size_t j = i + 1;
                while (j < one.size() && one[j] != '.' && !std::isspace(static_cast<unsigned char>(one[j]))) ++j;
                if (one.substr(i + 1, j - i - 1) != id) return false;
                i = j;
            } else if (one[i] == '.') {
                size_t j = i + 1;
                while (j < one.size() && one[j] != '.' && one[j] != '#' && !std::isspace(static_cast<unsigned char>(one[j]))) ++j;
                std::string want = one.substr(i + 1, j - i - 1);
                bool found = false;
                size_t k = 0;
                while (k < classes.size()) {
                    size_t e = classes.find(' ', k);
                    if (e == std::string::npos) e = classes.size();
                    if (classes.substr(k, e - k) == want) { found = true; break; }
                    k = e + 1;
                }
                if (!found) return false;
                i = j;
            } else {
                return false;  // unknown combinator in single segment
            }
        }
        return true;
    };
    // Strip child combinators and split on '>' too.
    std::stringstream ss(sel);
    std::string seg;
    bool first = true;
    while (ss >> seg) {
        if (!first && seg != ">" && seg != "+" && seg != "~") {
            // Descendant: any tag is fine.
        }
        if (seg == ">" || seg == "+" || seg == "~") continue;
        if (!matchOne(seg)) return false;
        first = false;
    }
    return true;
}

// Normalize a string for keyword comparison: case-fold (ASCII only,
// good enough for the CN/EN/anime titles we deal with) and collapse
// all whitespace runs to a single space.  This is what makes the
// subject-name match robust against the various ways HTML authors
// split a title across newlines / &nbsp; / multiple spaces.
std::string normalizeForKeyword(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool inWs = false;
    for (char c : s) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (std::isspace(u) || c == '\r' || c == '\n' || c == '\t') {
            if (!inWs && !out.empty()) { out.push_back(' '); inWs = true; }
        } else {
            out.push_back(static_cast<char>(std::tolower(u)));
            inWs = false;
        }
    }
    // Strip a single trailing space.
    if (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

// v15.4: findAnchor rewrite for prefetch hit rate.
//
// The old version had two silent failure modes that the smoke test
// surfaced — the 33 detail misses against 13 search HTMLs were
// mostly anchors whose visible text spanned newlines (the regex
// didn't use DOTALL, so multi-line text was missed) and whose
// characters used numeric HTML entities (the decoder only knew
// about the 6 named entities the spec lists, not `&#039;` or
// `&#x27;`).  This version:
//   * uses (?s) DOTALL on the anchor regex so the body of the
//     <a>...</a> can span newlines;
//   * strips every nested tag with a single replace-all instead
//     of a char-by-char state machine;
//   * decodes both named and numeric HTML entities;
//   * normalises whitespace + case-folds before comparing against
//     the subject keyword;
//   * does the keyword text match BEFORE the CSS selector filter
//     so a too-strict upstream selector can never silently drop a
//     valid subject link (we just log the selector mismatch in
//     debug instead).
std::string findAnchor(const std::string& html, const std::string& sel,
                       const std::string& subjectName,
                       std::string* outText = nullptr) {
    static const std::regex anchorRe(
        R"(<a\b([^>]*?)href=["']([^"']+)["']([^>]*?)>([\s\S]*?)</a\s*>)",
        std::regex::ECMAScript | std::regex::optimize | std::regex::icase);
    static const std::regex tagRe(R"(<[^>]+>)", std::regex::ECMAScript | std::regex::optimize);
    static const std::regex attrRe(
        R"(\b([a-zA-Z][a-zA-Z0-9_-]*)\s*=\s*["']([^"']*)["'])",
        std::regex::ECMAScript | std::regex::optimize);

    const std::string normKw = normalizeForKeyword(subjectName);

    // Two passes: first the strict (selector-matching) anchors, then
    // if we found nothing matching the keyword, accept any anchor
    // whose text contains the keyword regardless of selector — this
    // saves us when the upstream selector is "div.x > a" but the
    // page actually wraps the link in a different element.
    std::string fallback;
    std::string fallbackText;

    auto begin = std::sregex_iterator(html.begin(), html.end(), anchorRe);
    for (auto it = begin; it != std::sregex_iterator(); ++it) {
        const std::string preAttrs  = (*it)[1].str();
        const std::string href      = (*it)[2].str();
        const std::string postAttrs = (*it)[3].str();
        const std::string inner     = (*it)[4].str();
        // Skip nav anchors and similar dead ends.
        if (href.empty() || href[0] == '#' ||
            href.compare(0, 11, "javascript:") == 0) continue;

        // Pull class= and id= from the combined attribute string.
        std::string aClass, aId, aTag = "a";
        auto scanAttr = [&](const std::string& attrs) {
            auto ab = std::sregex_iterator(attrs.begin(), attrs.end(), attrRe);
            for (auto a = ab; a != std::sregex_iterator(); ++a) {
                std::string name = (*a)[1].str();
                std::transform(name.begin(), name.end(), name.begin(),
                               [](unsigned char c){ return std::tolower(c); });
                std::string val = (*a)[2].str();
                if (name == "class") aClass = val;
                else if (name == "id") aId = val;
            }
        };
        scanAttr(preAttrs);
        scanAttr(postAttrs);

        // Strip nested tags and decode entities.
        std::string text = decodeEntities(regex_replace(inner, tagRe, ""));
        text = trim(text);
        const std::string normText = normalizeForKeyword(text);
        if (normText.empty()) continue;

        // Keyword match (always normalised) — note the swap: we
        // do this BEFORE the selector check so that even if the
        // upstream CSS selector is too tight, a valid subject
        // link is still surfaced.
        if (!normKw.empty() &&
            normText.find(normKw) == std::string::npos &&
            normKw.find(normText) == std::string::npos) {
            continue;
        }

        const bool selectorOk = cssSelectorAccepts(sel, aTag, aId, aClass);
        if (selectorOk) {
            if (outText) *outText = text;
            return href;
        }
        // Selector said no but text matched — remember as fallback.
        // Prefer the fallback with the longest visible text (more
        // likely to be the actual subject entry than a small
        // sidebar chip).
        if (text.size() > fallbackText.size()) {
            fallback = href;
            fallbackText = text;
        }
    }
    if (!fallback.empty()) {
        if (outText) *outText = fallbackText;
        return fallback;
    }
    return {};
}

}  // namespace

WebSelectorProvider::WebSelectorProvider()
    : manifestUrl_(kDefaultAnimekoSourceUrl) {}

std::string WebSelectorProvider::readSearchCache(const std::string& sourceName,
                                                  const std::string& keyword) const {
    return cache::readSearchHtml(searchCacheDir_, sourceName, keyword);
}

std::string WebSelectorProvider::readDetailCache(const std::string& sourceName,
                                                  const std::string& keyword) const {
    return cache::readDetailHtml(searchCacheDir_, sourceName, keyword);
}

void WebSelectorProvider::setContext(int32_t episodeId,
                                     const std::string& subjectNameCN,
                                     const std::string& subjectName,
                                     double epSort,
                                     const std::string& episodeName) {
    std::lock_guard<std::mutex> lock(ctxMutex_);
    contexts_[episodeId] = Ctx{subjectNameCN, subjectName, epSort, episodeName};
}

void WebSelectorProvider::refreshManifest() {
    std::lock_guard<std::mutex> lock(manifestMutex_);
    manifestReady_ = false;
}

void WebSelectorProvider::ensureManifestLoaded() {
    if (manifestReady_.load()) return;
    if (manifestLoading_.exchange(true)) return;  // another thread is fetching
    auto url = manifestUrl_;
    brls::Logger::info("WebSelector: downloading manifest from {}", url);
    WebSelectorData out;
    bool ok = false;
    cpr::Session session;
    // v16.10.8.5: pre-resolve DNS — see image_helper.cpp.
    // v16.10.9.1: abort early if the helper can't resolve.
    std::string effectiveUrl, hostForHeader;
    if (HTTP::rewriteUrlForIP(url, effectiveUrl, hostForHeader) != 0) {
        brls::Logger::warning("WebSelector: manifest host not resolvable, abort");
        return;
    }
    session.SetUrl(cpr::Url{effectiveUrl});
    if (!hostForHeader.empty()) {
        session.UpdateHeader(cpr::Header{{"Host", hostForHeader}});
    }
    session.SetTimeout(cpr::Timeout{30000});
    auto r = session.Get();
    if (r.status_code == 200 && r.text.size() > 100) {
        ok = WebSelectorData::parse(r.text, out);
    }
    if (!ok) {
        // Fall back to the direct raw URL (in case the proxy is down).
        const std::string direct =
            "https://raw.githubusercontent.com/MajoSissi/animeko-source/main/dist/online.json";
        brls::Logger::warning("WebSelector: primary fetch failed ({}), retrying direct {}",
                               r.status_code, direct);
        cpr::Session s2;
        // v16.10.8.5: pre-resolve DNS — see image_helper.cpp.
        // v16.10.9.1: abort early if the helper can't resolve
        // either gh-proxy.com or raw.githubusercontent.com.
        std::string effectiveUrl2, hostForHeader2;
        if (HTTP::rewriteUrlForIP(direct, effectiveUrl2, hostForHeader2) != 0) {
            brls::Logger::warning("WebSelector: manifest fallback host not resolvable, abort");
            return;
        }
        s2.SetUrl(cpr::Url{effectiveUrl2});
        if (!hostForHeader2.empty()) {
            s2.UpdateHeader(cpr::Header{{"Host", hostForHeader2}});
        }
        s2.SetTimeout(cpr::Timeout{30000});
        auto r2 = s2.Get();
        if (r2.status_code == 200) {
            ok = WebSelectorData::parse(r2.text, out);
        }
    }
    {
        std::lock_guard<std::mutex> lock(manifestMutex_);
        if (ok) data_ = std::move(out);
        brls::Logger::info("WebSelector: parsed {} source(s)", data_.sources.size());
        manifestReady_ = true;
    }
    manifestLoading_ = false;
}

std::string WebSelectorProvider::absolutize(const std::string& base, const std::string& ref) {
    if (ref.empty()) return {};
    if (ref.find("http://") == 0 || ref.find("https://") == 0) return ref;
    if (ref[0] == '/') {
        // Same host as base.
        auto schemeEnd = base.find("://");
        if (schemeEnd == std::string::npos) return ref;
        auto hostEnd = base.find('/', schemeEnd + 3);
        if (hostEnd == std::string::npos) return base + ref;
        return base.substr(0, hostEnd) + ref;
    }
    // Relative to base's directory.
    auto slash = base.find_last_of('/');
    if (slash == std::string::npos) return ref;
    return base.substr(0, slash + 1) + ref;
}

std::string WebSelectorProvider::pickSubjectLink(const std::string& searchHtml,
                                                  const std::string& subjectName,
                                                  const WebSelectorSource& src) const {
    // The animeko-source selector for the search-results list is
    // typically "div.x a" or "div.x > a" 鈥?our findAnchor() understands
    // that shape and falls back to "any <a> with text" otherwise.
    return findAnchor(searchHtml, src.selectorSubjectA, subjectName);
}

std::string WebSelectorProvider::extractMediaUrl(const std::string& detailHtml,
                                                  const WebSelectorSource& src) const {
    if (!src.matchVideoUrl.mark_count()) return {};
    std::smatch m;
    auto begin = std::sregex_iterator(detailHtml.begin(), detailHtml.end(), src.matchVideoUrl);
    for (auto it = begin; it != std::sregex_iterator(); ++it) {
        if (!(*it).empty()) {
            return (*it)[0].str();
        }
    }
    return {};
}

void WebSelectorProvider::enumerate(int32_t episodeId,
                                     SourceListCb callback,
                                     std::function<void(const std::string&, int)> error) {
    ensureManifestLoaded();

    Ctx ctx;
    {
        std::lock_guard<std::mutex> lock(ctxMutex_);
        auto it = contexts_.find(episodeId);
        if (it == contexts_.end()) {
            if (error) error("WebSelector: no context for episode " + std::to_string(episodeId), -1);
            return;
        }
        ctx = it->second;
    }
    if (ctx.subjectNameCN.empty() && ctx.subjectName.empty()) {
        if (error) error("WebSelector: empty subject name in context", -2);
        return;
    }

    // Prefer the CN name (most animeko-source Chinese mirrors use CN
    // titles in their search results).
    const std::string subjectName = ctx.subjectNameCN.empty() ? ctx.subjectName : ctx.subjectNameCN;
    const std::string keyword =
        ctx.subjectNameCN.empty() && !ctx.subjectName.empty()
            ? ctx.subjectName
            : ctx.subjectNameCN;

    // Snapshot the source list under the lock, then drop it.
    std::vector<WebSelectorSource> sources;
    {
        std::lock_guard<std::mutex> lock(manifestMutex_);
        sources = data_.sources;
    }
    // Sort: lower tier first (= better), name as a stable tiebreaker.
    std::sort(sources.begin(), sources.end(),
              [](const WebSelectorSource& a, const WebSelectorSource& b) {
                  if (a.tier != b.tier) return a.tier < b.tier;
                  return a.name < b.name;
              });
    if (sources.size() > 12) sources.resize(12);  // cap: 12 sources 脳 ~2 reqs each
    // Bumped from 6 → 12 (2026-09-08) so the Switch has a better
    // chance of finding a playable m3u8 when many of the top-tier
    // sources are 502/timeout — the prefetch script (v15) caches
    // search+detail for every reachable source, so trying 12 of
    // them only costs 12*2=24 round-trips on the cache-fallback
    // path and zero on the cache-hit path.

    brls::Logger::info("WebSelector: searching '{}' ep#{} across {} source(s)",
                       keyword, ctx.epSort, sources.size());

    struct Pending { std::mutex m; size_t remain; std::vector<VideoSource> out; };
    auto pending = std::make_shared<Pending>();
    pending->remain = sources.size();

    auto finish = [pending, callback, error]() mutable {
        std::vector<VideoSource> snap;
        {
            std::lock_guard<std::mutex> lock(pending->m);
            if (--pending->remain != 0) return;
            snap = std::move(pending->out);
        }
        // Always succeed: empty vector is "no sources from this provider".
        if (callback) callback(std::move(snap));
    };

    for (size_t i = 0; i < sources.size(); ++i) {
        const auto& src = sources[i];
        const std::string searchUrl = src.searchUrl;
        // Replace {keyword} and any {keyword} case variations.
        std::string url = searchUrl;
        size_t pos = 0;
        while ((pos = url.find("{keyword}", pos)) != std::string::npos) {
            url.replace(pos, 10, urlEncodeKeyword(keyword));
            pos += urlEncodeKeyword(keyword).size();
        }
        if (url == searchUrl) {
            // No placeholder; the source presumably wants a fixed URL.
        }

        // Build a session with optional referer / user-agent / cookies.
        cpr::Session s;
        // v16.10.8.4: pre-resolve DNS — same root cause as
        // v16.10.8.2 / v16.10.8.3.  web_selector_provider talks
        // to 30+ upstream sources (mostly unknown to our
        // hardcoded table), so the raw UDP DNS path will do
        // most of the work; hardcoded misses are fine because
        // the helper falls through to the raw DNS resolver.
        // v16.10.9.1: when the helper returns -1 (neither
        // hardcoded nor raw DNS could resolve this host),
        // abort this source right here.  Without this, the
        // session is still created with the original hostname
        // URL and libcurl's internal gethostbyname still
        // hangs for 5-12s before timing out — 30+ sources
        // each potentially stuck for 5s when one host moves
        // is a 2-3 minute tail-end latency on the whole
        // web_selector walk.  Bailing out cleanly lets the
        // other sources proceed and the user sees a clear
        // "RESOLVE FAILED" line in startup.log for the
        // affected host.
        std::string effectiveUrl, hostForHeader;
        if (HTTP::rewriteUrlForIP(url, effectiveUrl, hostForHeader) != 0) {
            brls::Logger::warning("WebSelector: skipping source '{}' (host not resolvable)",
                                   src.name);
            finish();
            continue;
        }
        s.SetUrl(cpr::Url{effectiveUrl});
        if (!hostForHeader.empty()) {
            s.UpdateHeader(cpr::Header{{"Host", hostForHeader}});
        }
        s.SetTimeout(cpr::Timeout{15000});
        s.SetHeader(cpr::Header{{"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"},
                                  {"Accept-Language", "zh-CN,zh;q=0.9"}});
        if (!src.referer.empty())   s.SetHeader(cpr::Header{{"Referer", src.referer}});
        if (!src.userAgent.empty()) s.SetHeader(cpr::Header{{"User-Agent", src.userAgent}});
        if (!src.cookies.empty())   s.SetHeader(cpr::Header{{"Cookie", src.cookies}});

        // Cache-first: if the host has pre-fetched a search HTML for
        // (this source, this keyword) to disk, use it.  This is the
        // only way a JS-rendered upstream can work on the Switch.
        // If the live request succeeds, the live HTML wins; if it
        // fails, the cache becomes the fallback.
        const std::string cachedSearch = readSearchCache(src.name, keyword);
        const std::string cachedDetail = readDetailCache(src.name, keyword);

        // Two-step async: search -> extract detail URL -> GET detail -> extract m3u8.
        s.GetCallback([this, pending, src, finish, subjectName = keyword, cachedSearch, cachedDetail]
                     (const cpr::Response& r1) mutable {
            std::string searchHtml;
            if (r1.status_code == 200 && !r1.text.empty()) {
                searchHtml = r1.text;            // live wins
                brls::Logger::debug("WebSelector: '{}' search HTTP {} ({} B)",
                                    src.name, r1.status_code, searchHtml.size());
            } else if (!cachedSearch.empty()) {
                searchHtml = cachedSearch;        // fallback
                brls::Logger::info("WebSelector: '{}' search HTTP failed ({}), using cache",
                                   src.name, r1.status_code);
            } else {
                brls::Logger::info("WebSelector: '{}' search HTTP failed ({}), no cache",
                                   src.name, r1.status_code);
                return void(finish());
            }
            std::string detailHref = pickSubjectLink(searchHtml, subjectName, src);
            if (detailHref.empty()) {
                brls::Logger::debug("WebSelector: '{}' no <a> matched subject '{}' in search HTML",
                                    src.name, subjectName);
                return void(finish());
            }
            std::string detailUrl = absolutize(r1.url.str(), detailHref);
            if (detailUrl.empty()) detailUrl = detailHref;

            cpr::Session s2;
            // v16.10.8.5: pre-resolve DNS — see image_helper.cpp.
            // v16.10.9.1: abort detail fetch if host is not
            // resolvable — same logic as the manifest / per-
            // source search HTML above.  Skipping the cpr
            // session entirely saves a 5-12s libcurl DNS hang
            // for every dead source.
            std::string effectiveUrl3, hostForHeader3;
            if (HTTP::rewriteUrlForIP(detailUrl, effectiveUrl3, hostForHeader3) != 0) {
                brls::Logger::warning("WebSelector: detail host not resolvable, abort this source");
                return void(finish());
            }
            s2.SetUrl(cpr::Url{effectiveUrl3});
            if (!hostForHeader3.empty()) {
                s2.UpdateHeader(cpr::Header{{"Host", hostForHeader3}});
            }
            s2.SetTimeout(cpr::Timeout{15000});
            s2.SetHeader(cpr::Header{{"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"},
                                      {"Accept-Language", "zh-CN,zh;q=0.9"}});
            if (!src.referer.empty())   s2.SetHeader(cpr::Header{{"Referer", src.referer}});
            if (!src.userAgent.empty()) s2.SetHeader(cpr::Header{{"User-Agent", src.userAgent}});
            if (!src.cookies.empty())   s2.SetHeader(cpr::Header{{"Cookie", src.cookies}});
            std::string detailHtml;
            if (!cachedDetail.empty()) {
                // Cache hit: skip the detail HTTP entirely.  Most
                // animeko-source upstreams are JS-rendered SPAs that
                // would return an empty shell to a non-browser client,
                // so the cache is the only way the Switch can resolve
                // these sources at all.  The cache is read-only data
                // from a PC prefetch — safe to skip the network.
                detailHtml = cachedDetail;
                brls::Logger::info("WebSelector: '{}' detail cache hit, skipping HTTP",
                                   src.name);
            } else {
                auto r2 = s2.Get();
                if (r2.error || r2.status_code != 200) {
                    brls::Logger::info("WebSelector: '{}' detail HTTP failed ({}{}), no cache",
                                       src.name, r2.status_code,
                                       r2.error ? std::string(" ") + r2.error.message : std::string{});
                    return void(finish());
                }
                detailHtml = r2.text;
            }

            std::string m3u8 = extractMediaUrl(detailHtml, src);
            if (m3u8.empty()) {
                brls::Logger::info("WebSelector: '{}' matchVideoUrl regex produced no match (detail {} B)",
                                   src.name, detailHtml.size());
                return void(finish());
            }

            // Heuristically score the m3u8 URL by its quality hint.
            // Most anime CDNs encode the resolution into the path
            // (e.g. /1080p/index.m3u8 or /video/720p_playlist.m3u8)
            // or use /hd, /fhd markers.  Higher scores beat lower
            // scores in pickBest's tie-break, so a 1080p m3u8 from
            // tier-3 is preferred over a 480p m3u8 from tier-1.
            auto urlQuality = [](const std::string& u) -> int32_t {
                // Map a few common markers to a notional kbps
                // (it's an ordering score, not an actual measurement
                // — pickBest only uses this for sort).
                static const std::pair<const char*, int32_t> kMarkers[] = {
                    {"4k",     15000},
                    {"2160p",  15000},
                    {"fhd",     5000},
                    {"1080p",   5000},
                    {"1080",    5000},
                    {"hd",      2500},
                    {"720p",    2500},
                    {"720",     2500},
                    {"480p",    1000},
                    {"480",     1000},
                    {"sd",       600},
                    {"360p",     600},
                    {"360",      600},
                };
                // Scan lower-case copy to keep this allocation cheap.
                std::string lo(u.size(), '\0');
                for (size_t k = 0; k < u.size(); ++k) {
                    lo[k] = static_cast<char>(std::tolower(static_cast<unsigned char>(u[k])));
                }
                int32_t best = 0;
                for (const auto& m : kMarkers) {
                    const std::string needle = m.first;
                    if (lo.find(needle) != std::string::npos) {
                        if (m.second > best) best = m.second;
                    }
                }
                return best;
            };

            VideoSource vs;
            vs.kind  = SourceKind::HTTP;
            vs.url   = m3u8;
            vs.label = src.name;
            vs.priority = -src.tier;  // lower tier = higher priority
            vs.bitrate  = urlQuality(m3u8);  // 0 = unknown
            {
                std::lock_guard<std::mutex> lock(pending->m);
                pending->out.push_back(std::move(vs));
            }
            brls::Logger::info("WebSelector: '{}' resolved -> {} ({} chars)",
                               src.name, m3u8.substr(0, std::min<size_t>(m3u8.size(), 80)),
                               m3u8.size());
            finish();
        });
    }
}

}  // namespace aniswitch
