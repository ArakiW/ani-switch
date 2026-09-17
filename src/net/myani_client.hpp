// SPDX-License-Identifier: AGPL-3.0
//
// myani.org (Animeko's own danmaku relay, public instance at
// https://danmaku.api.myani.org/ for the global mirror and
// https://danmaku-cn.myani.org/ for the China mirror) — REST, NOT
// WebSocket.  The earlier danmaku_ws.hpp that talked about JSON frames
// over a WS handshake was speculative; the actual server protocol is
// plain HTTPS (per open-ani/animeko's AniDanmakuProvider /
// AniDanmakuSender / danmaku.protocol.*).
//
// Endpoints used:
//
//   POST /v1/login
//     body:    { "token": "<Bangumi PAT>" }
//     reply:   { "token": "<myani session token>", "selfInfo": { ... } }
//
//   GET  /v1/danmaku/<episodeId>
//     reply:   { "danmakuList": [
//                { "id": 12345,
//                  "senderId": "...",
//                  "danmakuInfo": {
//                      "playTime": 12.345,   // seconds (Double)
//                      "location":  "TOP" | "BOTTOM" | "NORMAL" | "ADV",
//                      "text":      "...",
//                      "color":     16777215
//                  }
//                },
//                ...
//              ] }
//
//   POST /v1/danmaku/<episodeId>
//     headers: Authorization: Bearer <myani session token>
//     body:    { "danmakuInfo": { "playTime", "location", "text", "color" } }
//
// All calls are plain HTTPS; only POST sendDanmaku requires auth.
//
// For "live" behaviour (continuous display during playback) we
// re-fetch the recent tail every 5s — there's no push channel. The
// DanmakuRenderer orchestrates that by calling fetchDanmaku()
// periodically with the current playback time and diffing against the
// last seen id.

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "net/myani_types.hpp"   // MyaniDanmaku, MyaniLocation, toParsed()
#include "net/http.hpp"         // ErrorCallback, fireError

namespace aniswitch {

class MyaniClient {
public:
    using DanmakuListCb = std::function<void(std::vector<MyaniDanmaku>)>;
    using SendCb        = std::function<void(MyaniDanmaku /*echoed*/)>;
    using LoginCb       = std::function<void(std::string /*myani session token*/,
                                              std::string /*username*/)>;

    MyaniClient();

    // Process-wide singleton.  The myani client is stateless beyond
    // the cached session token + base url, and the player activity
    // fires requests from its own callbacks, so a single instance is
    // enough and avoids cross-thread config drift.
    static MyaniClient& instance();

    // Base URL.  Defaults to danmaku-cn.myani.org; the global mirror
    // is danmaku.api.myani.org.  Call this before any other API.
    void setBaseUrl(const std::string& url);

    // Base URL getter (mainly for diagnostics + tests).
    const std::string& baseUrl() const { return baseUrl_; }

    // Configure the Bangumi PAT used for both login and to authorise
    // POST /v1/danmaku.  Pass an empty string to clear.
    void setBangumiToken(const std::string& pat);

    // Has the client successfully logged in (i.e. is there a myani
    // session token currently cached)?  This does not imply the token
    // is still valid server-side — it just means we believe it.
    bool hasSession() const;

    // The cached session token (empty if none).  Useful for tests.
    const std::string& sessionToken() const { return sessionToken_; }

    // Exchange a Bangumi PAT for a myani session token (POST /v1/login).
    // The result is cached and used for subsequent sendDanmaku() calls.
    // Safe to call again to refresh.
    void login(LoginCb callback = nullptr, ErrorCallback error = nullptr);

    // GET /v1/danmaku/<episodeId>.  No auth required.
    void fetchDanmaku(int64_t episodeId,
                      DanmakuListCb callback = nullptr,
                      ErrorCallback error = nullptr);

    // POST /v1/danmaku/<episodeId> with the cached myani session.
    // If no session is cached, the callback's error path is invoked
    // with a synthetic "not logged in" error.  Caller may pre-emptively
    // call login() to ensure the session is fresh.
    void sendDanmaku(int64_t episodeId,
                    const std::string& text,
                    MyaniLocation location = MyaniLocation::NORMAL,
                    int32_t color = 0xFFFFFF,
                    SendCb callback = nullptr,
                    ErrorCallback error = nullptr);

private:
    void doLogin(const std::string& pat, LoginCb cb, ErrorCallback err);
    void doFetch(int64_t episodeId, DanmakuListCb cb, ErrorCallback err);
    void doSend(int64_t episodeId, const std::string& text,
                MyaniLocation location, int32_t color,
                SendCb cb, ErrorCallback err);

    std::string baseUrl_;
    std::string bangumiToken_;
    std::string sessionToken_;
    std::string selfUsername_;
    mutable std::mutex mu_;       // protects the three string fields
    std::atomic<bool> loginInFlight_{false};
};

}  // namespace aniswitch
