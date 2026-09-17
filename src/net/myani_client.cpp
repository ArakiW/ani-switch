// SPDX-License-Identifier: AGPL-3.0

#include "net/myani_client.hpp"
#include <cpr/cpr.h>
#include <borealis/core/logger.hpp>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace aniswitch {

namespace {
    const cpr::Timeout MYANI_TIMEOUT{5000};

    MyaniLocation parseLocation(const std::string& s) {
        if (s == "TOP")    return MyaniLocation::TOP;
        if (s == "BOTTOM") return MyaniLocation::BOTTOM;
        if (s == "ADV")    return MyaniLocation::ADV;
        return MyaniLocation::NORMAL;
    }

    cpr::Header myaniHeaders(const std::string& sessionToken) {
        cpr::Header h = HTTP::HEADERS;
        h["User-Agent"] = "ani-switch/0.1.0 (https://github.com/MiniMax139102/ani-switch)";
        h["Accept"]     = "application/json";
        if (!sessionToken.empty()) {
            h["Authorization"] = "Bearer " + sessionToken;
        }
        return h;
    }

    // Parse the danmaku list response into our domain type.
    // myani's per-danmaku shape is:
    //   { "id": 12345,
    //     "senderId": "...",
    //     "danmakuInfo": {
    //         "playTime": 12.345,
    //         "location": "NORMAL",
    //         "text":      "...",
    //         "color":     16777215 } }
    std::vector<MyaniDanmaku> parseList(const nlohmann::json& j) {
        std::vector<MyaniDanmaku> out;
        if (!j.is_object() || !j.contains("danmakuList") || !j["danmakuList"].is_array()) {
            return out;
        }
        out.reserve(j["danmakuList"].size());
        for (const auto& item : j["danmakuList"]) {
            MyaniDanmaku d;
            if (item.contains("id") && item["id"].is_number_integer()) {
                d.id = item["id"].get<int64_t>();
            }
            if (item.contains("senderId") && item["senderId"].is_string()) {
                d.senderId = item["senderId"].get<std::string>();
            }
            if (item.contains("danmakuInfo") && item["danmakuInfo"].is_object()) {
                const auto& info = item["danmakuInfo"];
                if (info.contains("playTime") && info["playTime"].is_number()) {
                    d.playTimeMs = static_cast<int64_t>(info["playTime"].get<double>() * 1000.0);
                }
                if (info.contains("color") && info["color"].is_number_integer()) {
                    d.color = info["color"].get<int32_t>();
                }
                if (info.contains("location") && info["location"].is_string()) {
                    d.location = parseLocation(info["location"].get<std::string>());
                }
                if (info.contains("text") && info["text"].is_string()) {
                    d.text = info["text"].get<std::string>();
                }
            }
            out.push_back(std::move(d));
        }
        return out;
    }
}  // namespace

MyaniClient::MyaniClient() {
    std::lock_guard<std::mutex> lk(mu_);
    if (baseUrl_.empty()) baseUrl_ = "https://danmaku-cn.myani.org";
}

MyaniClient& MyaniClient::instance() {
    static MyaniClient c;
    return c;
}

void MyaniClient::setBaseUrl(const std::string& url) {
    std::lock_guard<std::mutex> lk(mu_);
    baseUrl_ = url;
}

void MyaniClient::setBangumiToken(const std::string& pat) {
    std::lock_guard<std::mutex> lk(mu_);
    bangumiToken_ = pat;
    // Forcing re-login when the user changes their token keeps the
    // session in sync.  Without this, an old session from a previous
    // PAT would silently be used to send danmaku under a new identity.
    sessionToken_.clear();
    selfUsername_.clear();
}

bool MyaniClient::hasSession() const {
    std::lock_guard<std::mutex> lk(mu_);
    return !sessionToken_.empty();
}

void MyaniClient::login(LoginCb callback, ErrorCallback error) {
    std::string pat;
    {
        std::lock_guard<std::mutex> lk(mu_);
        pat = bangumiToken_;
    }
    if (pat.empty()) {
        fireError(error, "myani login: no Bangumi PAT configured", -2);
        return;
    }
    // Don't re-issue a login while one is in flight — myani rate-limits
    // the endpoint, and a slow first call shouldn't trigger a second
    // from the same client.
    bool expected = false;
    if (!loginInFlight_.compare_exchange_strong(expected, true)) {
        fireError(error, "myani login already in progress", -3);
        return;
    }
    doLogin(pat,
            [this, callback](std::string token, std::string username) {
                loginInFlight_.store(false);
                if (callback) callback(std::move(token), std::move(username));
            },
            [this, error](const std::string& msg, int code) {
                loginInFlight_.store(false);
                fireError(error, msg, code);
            });
}

void MyaniClient::doLogin(const std::string& pat, LoginCb cb, ErrorCallback err) {
    const std::string url = [this] {
        std::lock_guard<std::mutex> lk(mu_);
        return baseUrl_ + "/v1/login";
    }();
    auto session = HTTP::createSession();
    // v16.10.8.3: pre-resolve DNS.
    std::string effectiveUrl, hostForHeader;
    HTTP::rewriteUrlForIP(url, effectiveUrl, hostForHeader);
    session->SetUrl(cpr::Url{effectiveUrl});
    session->SetHeader(myaniHeaders({}));
    if (!hostForHeader.empty()) {
        session->UpdateHeader(cpr::Header{{"Host", hostForHeader}});
    }
    session->SetTimeout(MYANI_TIMEOUT);
    const std::string body = fmt::format(R"({{"token":"{}"}})", pat);
    session->SetBody(cpr::Body{body});
    // v16.10.8.1: UpdateHeader (merge) instead of SetHeader
    // (replace) so the UA / Accept installed by myaniHeaders()
    // above aren't wiped out.
    session->UpdateHeader(cpr::Header{{"Content-Type", "application/json"}});
    session->PostCallback([this, cb, err](const cpr::Response& r) {
        if (r.error) { fireError(err, "myani login: " + r.error.message, -1); return; }
        if (r.status_code != 200) {
            fireError(err, fmt::format("myani login HTTP {}", r.status_code), r.status_code);
            return;
        }
        try {
            auto j = nlohmann::json::parse(r.text);
            if (j.contains("errorCode") && j["errorCode"].is_number_integer()
                && j["errorCode"].get<int>() != 0) {
                const std::string msg = j.contains("errorMessage") && j["errorMessage"].is_string()
                    ? j["errorMessage"].get<std::string>() : "unknown";
                fireError(err, "myani login: " + msg, j["errorCode"].get<int>());
                return;
            }
            const std::string token   = j.value("token", "");
            std::string username;
            if (j.contains("selfInfo") && j["selfInfo"].is_object()
                && j["selfInfo"].contains("username")
                && j["selfInfo"]["username"].is_string()) {
                username = j["selfInfo"]["username"].get<std::string>();
            }
            if (token.empty()) {
                fireError(err, "myani login: response missing token", -4);
                return;
            }
            {
                std::lock_guard<std::mutex> lk(mu_);
                sessionToken_ = token;
                selfUsername_ = username;
            }
            brls::Logger::info("myani: logged in as {}", username);
            if (cb) cb(token, username);
        } catch (const std::exception& e) {
            fireError(err, std::string("myani login: bad JSON: ") + e.what(), -5);
        }
    });
}

void MyaniClient::fetchDanmaku(int64_t episodeId, DanmakuListCb callback, ErrorCallback error) {
    if (episodeId <= 0) {
        fireError(error, "myani fetch: invalid episodeId", -6);
        return;
    }
    doFetch(episodeId,
            [callback](std::vector<MyaniDanmaku> list) {
                if (callback) callback(std::move(list));
            },
            error);
}

void MyaniClient::doFetch(int64_t episodeId, DanmakuListCb cb, ErrorCallback err) {
    const std::string url = [this, episodeId] {
        std::lock_guard<std::mutex> lk(mu_);
        return baseUrl_ + fmt::format("/v1/danmaku/{}", episodeId);
    }();
    auto session = HTTP::createSession();
    // v16.10.8.3: pre-resolve DNS — see HTTP::rewriteUrlForIP /
    // AuthedHTTP::authedGet (v16.10.8.2).  Without this, every
    // myani danmaku request still hits cpr's newlib gethostbyname
    // and hangs in hbmenu applet mode.
    std::string effectiveUrl, hostForHeader;
    HTTP::rewriteUrlForIP(url, effectiveUrl, hostForHeader);
    session->SetUrl(cpr::Url{effectiveUrl});
    session->SetHeader(myaniHeaders({}));
    if (!hostForHeader.empty()) {
        session->UpdateHeader(cpr::Header{{"Host", hostForHeader}});
    }
    session->SetTimeout(MYANI_TIMEOUT);
    session->GetCallback([cb, err](const cpr::Response& r) {
        if (r.error) { fireError(err, "myani fetch: " + r.error.message, -1); return; }
        if (r.status_code != 200) {
            fireError(err, fmt::format("myani fetch HTTP {}", r.status_code), r.status_code);
            return;
        }
        try {
            const auto list = parseList(nlohmann::json::parse(r.text));
            brls::Logger::debug("myani: fetched {} danmaku for episode", list.size());
            if (cb) cb(std::move(list));
        } catch (const std::exception& e) {
            fireError(err, std::string("myani fetch: bad JSON: ") + e.what(), -5);
        }
    });
}

void MyaniClient::sendDanmaku(int64_t episodeId, const std::string& text,
                              MyaniLocation location, int32_t color,
                              SendCb callback, ErrorCallback error) {
    if (episodeId <= 0) { fireError(error, "myani send: invalid episodeId", -6); return; }
    if (text.empty())   { fireError(error, "myani send: empty text", -7); return; }
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (sessionToken_.empty()) {
            fireError(error, "myani send: not logged in", -8);
            return;
        }
    }
    doSend(episodeId, text, location, color,
           [callback](MyaniDanmaku echoed) {
               if (callback) callback(std::move(echoed));
           },
           error);
}

void MyaniClient::doSend(int64_t episodeId, const std::string& text,
                         MyaniLocation location, int32_t color,
                         SendCb cb, ErrorCallback err) {
    std::string url, sessionToken;
    {
        std::lock_guard<std::mutex> lk(mu_);
        url = baseUrl_ + fmt::format("/v1/danmaku/{}", episodeId);
        sessionToken = sessionToken_;
    }
    // playTime in the request is "current playback time in seconds";
    // myani uses Double.  Caller-supplied 0 means "send now"; the
    // server-side stores the timestamp from "now" if the field is
    // missing, but we always include it for consistency.
    const std::string body = fmt::format(
        R"({{"danmakuInfo":{{"playTime":0,"location":"{}","text":"{}","color":{}}}}})",
        myaniLocationToString(location), text, color);

    auto session = HTTP::createSession();
    // v16.10.8.3: pre-resolve DNS.
    std::string effectiveUrl, hostForHeader;
    HTTP::rewriteUrlForIP(url, effectiveUrl, hostForHeader);
    session->SetUrl(cpr::Url{effectiveUrl});
    session->SetHeader(myaniHeaders(sessionToken));
    if (!hostForHeader.empty()) {
        session->UpdateHeader(cpr::Header{{"Host", hostForHeader}});
    }
    // v16.10.8.1: UpdateHeader (merge) — see login path above.
    session->UpdateHeader(cpr::Header{{"Content-Type", "application/json"}});
    session->SetTimeout(MYANI_TIMEOUT);
    session->SetBody(cpr::Body{body});
    session->PostCallback([cb, err, text, location, color](const cpr::Response& r) {
        if (r.error) { fireError(err, "myani send: " + r.error.message, -1); return; }
        if (r.status_code != 200) {
            fireError(err, fmt::format("myani send HTTP {}", r.status_code), r.status_code);
            return;
        }
        MyaniDanmaku echoed;
        echoed.text     = text;
        echoed.color    = color;
        echoed.location = location;
        // The server's response shape is the same as the GET list item,
        // so the caller can match by id.  We don't try to parse the
        // response here — the only "field" that matters is whether the
        // status is 2xx, and we just asserted that.  A future commit
        // can fill in the actual echo if needed.
        brls::Logger::info("myani: sent danmaku \"{}\" ({} bytes)", text, text.size());
        if (cb) cb(echoed);
    });
}

}  // namespace aniswitch
