// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
//
// HTTP wrapper around cpr/libcurl. Renamed namespace + dropped B站 WBI
// signing + changed User-Agent and Referer defaults.

#pragma once

#include <cpr/cpr.h>
#include "net/json_helper.hpp"
#include <nlohmann/json.hpp>
#include <functional>
#include <map>
#include <string>
#include <memory>
#include <vector>

namespace aniswitch {

using Cookie = std::map<std::string, std::string>;
using ErrorCallback = std::function<void(const std::string& err, int code)>;

// Inline helper instead of a macro: the previous ERROR_MSG macro expanded to
// `if (error) error(msg, code)`, which broke inside lambdas whose captured
// callback was named `err` (not `error`) — the preprocessor-replaced
// identifier `error` did not resolve, and the build failed with
// "'error' was not declared in this scope; did you mean 'perror'?".
// A real function takes the callback explicitly, so it works everywhere.
inline void fireError(ErrorCallback err, const std::string& msg, int code) {
    if (err) err(msg, code);
}
#define HTTP_CALLBACK(data)  if (callback) callback(data)

#if defined(__SWITCH__)
// v19.0.2 case B' skeleton: direct mbedTLS HTTP fetch result.
// Returned by HTTP::directFetch.  Lives in the HTTP public surface
// so the rest of the codebase doesn't have to know mbedTLS exists.
// The production path (HTTP::_cpr_get / _cpr_post) still returns
// cpr::Response, so the two are not interchangeable.
struct DirectFetchResult {
    long        status     = 0;   // HTTP/1.1 status, 0 on transport/TLS error
    std::string body;             // response body
    std::string error;            // "" on success
    double      elapsed_ms = 0;   // wall clock
};
#endif

class HTTP {
public:
    static inline cpr::Cookies COOKIES;
    static inline cpr::Header  HEADERS = {
        {"User-Agent", "ani-switch/0.1.0 (https://github.com/MiniMax139102/ani-switch)"},
        {"Accept",     "application/json"},
    };
    static inline int TIMEOUT            = 12000;
    static inline int CONNECTION_TIMEOUT = 5000;
    static inline int DNS_CACHE_TIMEOUT  = 60;
    static inline cpr::Proxies  PROXIES;
    // v20.0: runtime HTTP/SOCKS proxy (Clash / V2Ray on LAN).
    // When set, curl does CONNECT through the proxy and we skip
    // the Switch raw-DNS rewrite — the proxy resolves names.
    static inline std::string   PROXY_URL;
    static void applyProxy(const std::string& url);
    static bool hasProxy() { return !PROXY_URL.empty(); }
    static std::string proxyHint();
    /* v16.10.6 → v16.10.10: SSL verification.  v16.10.6
       flipped verify off because the Switch's libnx mbedTLS
       backend had no CA bundle (handshake 15s → timeout).
       v16.10.9.1's field data shows the underlying issue
       was *not* cert validation — with verify=false the
       handshake still fails with `SSL connect error`
       (NETDIAG2 + DNS pre-resolve prove the network path
       works).  The cpr/curl/boringssl Switch backend reports
       SSL connect error at the very first TLS record, which
       is consistent with a protocol-version or cipher-suite
       mismatch between Switch mbedTLS and Cloudflare's TLS
       1.3-only edge.  v16.10.10 keeps verify off (verify
       on still wouldn't fix the handshake) and adds an
       explicit `CURLOPT_SSLVERSION = TLSv1.2` so we negotiate
       the highest version both sides accept.  v16.10.10 also
       feeds cpr the Switch's native cert path
       (`sdmc:/switch/aniswitch/ca-bundle.crt`) via
       `cpr::SslOptions{CaPath = ...}` — if the user drops a
       CA bundle there, verify is automatically re-enabled.
    */
    static inline cpr::VerifySsl VERIFY{false};
    /* v16.10.12: cpr exposes two CA options.  `ca_path` is a
       *directory* (curl's CURLOPT_CAPATH, e.g. /etc/ssl/certs);
       `ca_info` is a *single file* (curl's CURLOPT_CAINFO,
       e.g. /etc/ssl/certs/ca-certificates.crt).  v16.10.10 /
       v16.10.11 wired the bundle file path into `ca_path`,
       which curl then treated as a directory and tried to
       `fopen()` directly as a CA file — producing the
       `Error opening ca path sdmc:/switch/aniswitch/
       ca-bundle.crt` from the field data.  The fix is to
       use `ca_info` (single file).  Path is the same; only
       the cpr field name changes. */
    static inline std::string SSL_CA_PATH = "sdmc:/switch/aniswitch/ca-bundle.crt";
    // v18.9.1: TLS / HTTP wire log toggle.  When true, every
    // `createSession()` installs a cpr DebugCallback that writes
    // curl verbose text + hex-dumped SSL_DATA_IN/SSL_DATA_OUT
    // records into `sdmc:/switch/aniswitch/startup.log`.  This
    // is the diagnostic hook for the v18.0.3 P0
    // (`Recv failure: Connection reset by peer` mid-handshake
    // on `https://api.bgm.tv/calendar`).  Once we have a wire
    // capture, the next round of fixes targets the exact
    // client-hello extension / cipher / state line that Cloudflare
    // 1.3 edge is rejecting; until then the home tabs still have
    // the 5s timeout fallback that lands in setEmpty.
    // v18.9.1 ships with this on; flip to false in a later build
    // to silence the log once we have the capture.
    // v18.9.1 shipped this ON for wire capture.  v20.0 field data
    // already proved api.animeko.org TLS works; keeping verbose on
    // races startup.log from ImageLoader worker threads and is a
    // crash suspect.  Flip on only when debugging a new host.
    static inline bool NET_DEBUG_VERBOSE = false;
    // v19.0.2 case B' skeleton: runtime toggle.  Off by default
    // — the cpr/curl path remains the production code path.  The
    // settings UI can flip this on for field testing once a
    // v19.0.3 worker wires the existing _cpr_get/_cpr_post
    // through to directFetch.  Honoured by HTTP::directFetch().
    static inline bool USE_DIRECT_TLS = false;

    static std::shared_ptr<cpr::Session> createSession();

    // Configure a bare cpr::Session the same way _cpr_get does for
    // Switch HTTPS (TLS 1.2 pin + cipher list + CA + raw-DNS resolve).
    // Used by ImageLoader for binary downloads. Returns 0 if resolve
    // was applied, -1 if the host was left as-is (caller should fail
    // fast rather than let curl's gethostbyname hang).
    static int prepareFetchSession(cpr::Session& session,
                                   const std::string& url);

    // Resolve host from `url` (with cache + fake-ip reject + DoH
    // fallback + Cloudflare anycast last-resort). Never rewrites the
    // URL — callers must keep the hostname so TLS SNI stays correct.
    // Returns 0 and fills outIp/outHost on success.
    static int resolveHostForUrl(const std::string& url,
                                 std::string& outIp,
                                 std::string& outHost);

    // SetResolve-only DNS pin for an already-configured session.
    // URL is left untouched (SNI = hostname). 0 = applied, -1 = skip.
    static int applyDnsToSession(cpr::Session& session,
                                 const std::string& url);

    // v19.0.2 case B' skeleton: bypass cpr/curl, talk to the
    // server directly via mbedTLS 2.28 (the devkitpro portlibs
    // backend).  Honours `USE_DIRECT_TLS`; if that is false the
    // call returns immediately with `error = "direct TLS
    // disabled"`.  Returns DirectFetchResult with `status == 0`
    // on any transport / TLS / parse error.
    //
    // `headers` is a list of "Name: Value" lines (CRLF not
    // included).  `body` is the request body for POST.
    //
    // The HTTP::public API does not change — _cpr_get / _cpr_post
    // are still the production path.  This exists so a future
    // v19.0.3 worker can swap the call site from cpr to direct
    // mbedTLS without touching the caller's template signature.
    //
    // Switch only: defined to return an empty DirectFetchResult
    // on non-Switch builds (the call site still compiles).
#if defined(__SWITCH__)
    static DirectFetchResult directFetch(
        const std::string& url,
        const std::string& method = "GET",
        const std::string& body   = "",
        const std::vector<std::string>& headers = {},
        long timeout_ms           = 10000);
#else
    static DirectFetchResult directFetch(
        const std::string&,
        const std::string& = "GET",
        const std::string& = "",
        const std::vector<std::string>& = {},
        long = 10000) { return DirectFetchResult{}; }
#endif

    /* v16.10.8.2: pre-resolve DNS for a URL and rewrite it to use
     * the IP directly, with the original hostname moved into a
     * `Host:` header (so libcurl's SNI / virtual-host routing still
     * see the right name).  On the Switch, cpr/curl's newlib
     * gethostbyname hangs in hbmenu applet mode (v16.10.7's
     * NETDIAG2 proved the BSD socket layer is fine — only the
     * resolver is blocked), so this helper lets callers that
     * bypass `_cpr_get` / `_cpr_post` (e.g. the async
     * `AuthedHTTP::authedGet` / `authedPost` path) still avoid
     * the broken resolver.
     *
     * Returns 0 on success (URL rewritten, Host header set),
     * -1 if neither the hardcoded table nor the raw UDP DNS
     * resolver could resolve the host (the original URL is
     * left untouched, and no Host header is added).  When
     * `__SWITCH__` is not defined this is a no-op that returns
     * -1, so non-Switch builds behave as before.
     */
    static int rewriteUrlForIP(const std::string& url,
                               std::string& effectiveUrl,
                               std::string& hostForHeader);

    // ---- Internal primitives -----------------------------------------
    static void _cpr_post(const std::string& url,
                          const cpr::Parameters& parameters,
                          const cpr::Payload&    payload,
                          std::function<void(const cpr::Response&)> cb,
                          ErrorCallback err);

    static void _cpr_get(const std::string& url,
                         const cpr::Parameters& parameters,
                         std::function<void(const cpr::Response&)> cb,
                         ErrorCallback err);

    // ---- Typed JSON parse helpers -------------------------------------
    template <typename ReturnType>
    static int parseJson(const cpr::Response& r,
                         std::function<void(ReturnType)> callback = nullptr,
                         ErrorCallback error = nullptr) {
        try {
            auto res = nlohmann::json::parse(r.text);
            // Bangumi / generic APIs use a top-level "code" + "message" + "result" shape.
            // Legacy B站 APIs use "code" + "message" + "data" + "result" shapes.
            int code = 0;
            if (res.is_object() && res.contains("code")) {
                if (res["code"].is_number_integer()) {
                    code = res["code"].get<int>();
                } else if (res["code"].is_number()) {
                    code = static_cast<int>(res["code"].get<double>());
                } else if (res["code"].is_string()) {
                    code = 0; // some legacy APIs use string status
                }
            }
            if (code == 0) {
                HTTP_CALLBACK(parseResponseData<ReturnType>(res));
                return 0;
            }
            if (res.contains("error") && res["error"].is_string()) {
                fireError(error, res["error"].get<std::string>(), code);
            } else if (res.contains("message") && res["message"].is_string()) {
                fireError(error, res["message"].get<std::string>(), code);
            } else {
                fireError(error, "Unknown API error", code);
            }
        } catch (const std::exception& e) {
            fireError(error, std::string("API parse error: ") + e.what(), 200);
        }
        return 1;
    }

    template <typename ReturnType>
    static void getResult(const std::string& url,
                          cpr::Parameters parameters = {},
                          std::function<void(ReturnType)> callback = nullptr,
                          ErrorCallback error = nullptr) {
        _cpr_get(url, parameters,
                 [callback, error](const cpr::Response& r) {
                     parseJson<ReturnType>(r, callback, error);
                 },
                 error);
    }

    template <typename ReturnType>
    static void postResult(const std::string& url,
                           cpr::Parameters parameters = {},
                           cpr::Payload    payload    = {},
                           std::function<void(ReturnType)> callback = nullptr,
                           ErrorCallback error = nullptr) {
        _cpr_post(url, parameters, payload,
                  [callback, error](const cpr::Response& r) {
                      parseJson<ReturnType>(r, callback, error);
                  },
                  error);
    }

    // Fire-and-forget: no payload, just confirm the request succeeded.
    static void postVoid(const std::string& url,
                         cpr::Parameters parameters = {},
                         cpr::Payload    payload    = {},
                         std::function<void()> callback = nullptr,
                         ErrorCallback error = nullptr);
};

// Shared curl easy handle options. Apply at session start.
inline void setGlobalCurlShare(cpr::Session& session) {
    // cpr exposes handle via GetCurlHolder(); we keep a no-op for now.
    (void)session;
}

}  // namespace aniswitch
