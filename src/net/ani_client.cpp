// SPDX-License-Identifier: AGPL-3.0

#include "net/ani_client.hpp"
#include "net/http.hpp"
#include "net/json_helper.hpp"
#include "ui/presenter/presenter.hpp"  // for uiCallback
#include <borealis/core/logger.hpp>
#include <cstdio>
#include <ctime>
#include <fmt/format.h>

#if defined(__SWITCH__)
extern "C" void aniswitchStartupLog(const char* message);
#endif

namespace aniswitch {

namespace {

// 2026-09-09: resolved from the animeko 6.1.0 OpenAPI generator
// (`ApiClient.BASE_URL = "https://api.animeko.org"`).  Cloudflare
// edge in front of an Asia-Pacific origin.
constexpr const char* kAniBase = "https://api.animeko.org";

void from_json(const nlohmann::json& j, AniOtpResponse& v) {
    readJsonField(j, "otpId",            v.otpId);
    if (j.contains("hasExistingUser") && j["hasExistingUser"].is_boolean()) {
        v.hasExistingUser = j["hasExistingUser"].get<bool>();
    }
}

void from_json(const nlohmann::json& j, AniTokens& v) {
    readJsonField(j, "accessToken",      v.accessToken);
    readJsonField(j, "refreshToken",     v.refreshToken);
    if (j.contains("expiresAtMillis") && j["expiresAtMillis"].is_number_integer()) {
        v.expiresAtMillis = j["expiresAtMillis"].get<int64_t>();
    }
    if (j.contains("bangumiAccessToken") && j["bangumiAccessToken"].is_string()) {
        readJsonField(j, "bangumiAccessToken", v.bangumiPat);
    }
}

void from_json(const nlohmann::json& j, AniLoginResponse& v) {
    readJsonField(j, "userId", v.userId);
    // Manual descent into "tokens" because nlohmann::json's
    // template-based get<>() needs an accessible from_json for
    // AniTokens in the same translation unit, which the
    // ADL-friendly helper above already provides — but the
    // call site has to spell out the type.  Calling the helper
    // recursively does the trick and keeps the JSON layout in
    // one place.
    if (j.contains("tokens") && j["tokens"].is_object()) {
        from_json(j["tokens"], v.tokens);
    }
}

}  // namespace

void AniClient::sendEmailOtp(const std::string& email,
                             OtpCb callback, ErrorCb error) {
    nlohmann::json body = {
        {"email",    email},
        {"language", nullptr},
    };
    // cpr::Payload takes an initializer_list of Pair, not a raw
    // string body, so we can't route through HTTP::postResult for
    // a JSON POST.  Build a Session inline with SetBody() and
    // a JSON Content-Type.  The session is freed when this
    // function returns (cpr's shared_ptr model).
    auto session = HTTP::createSession();
    session->SetUrl(cpr::Url{std::string(kAniBase) + "/v2/users/auth/email/otp"});
    session->SetBody(cpr::Body{body.dump()});
    session->SetHeader(cpr::Header{
        {"Content-Type", "application/json"},
        {"User-Agent", "ani-switch/0.1.0 (https://github.com/MiniMax139102/ani-switch)"},
        {"Accept", "application/json"},
    });
    cpr::Response r = session->Post();
    if (r.error) {
        brls::Logger::warning("AniClient::sendEmailOtp: {} (code={})",
                              r.error.message, -1);
        if (error) error(r.error.message, -1);
        return;
    }
    if (r.status_code != 200) {
        if (error) error(std::string("HTTP ") + std::to_string(r.status_code),
                          r.status_code);
        return;
    }
    try {
        auto j = nlohmann::json::parse(r.text);
        AniOtpResponse out;
        from_json(j, out);
        if (callback) callback(std::move(out));
    } catch (const std::exception& e) {
        if (error) error(std::string("parse: ") + e.what(), -1);
    }
}

void AniClient::loginByEmailOtp(const std::string& otpId,
                                const std::string& otpValue,
                                LoginCb callback, ErrorCb error) {
    nlohmann::json body = {
        {"otpId",    otpId},
        {"otpValue", otpValue},
    };
    auto session = HTTP::createSession();
    session->SetUrl(cpr::Url{std::string(kAniBase) + "/v2/users/auth/email"});
    session->SetBody(cpr::Body{body.dump()});
    session->SetHeader(cpr::Header{
        {"Content-Type", "application/json"},
        {"User-Agent", "ani-switch/0.1.0 (https://github.com/MiniMax139102/ani-switch)"},
        {"Accept", "application/json"},
    });
    cpr::Response r = session->Post();
    if (r.error) {
        brls::Logger::warning("AniClient::loginByEmailOtp: {} (code={})",
                              r.error.message, -1);
        if (error) error(r.error.message, -1);
        return;
    }
    if (r.status_code != 200) {
        if (error) error(std::string("HTTP ") + std::to_string(r.status_code),
                          r.status_code);
        return;
    }
    try {
        auto j = nlohmann::json::parse(r.text);
        AniLoginResponse out;
        from_json(j, out);
        if (callback) callback(std::move(out));
    } catch (const std::exception& e) {
        if (error) error(std::string("parse: ") + e.what(), -1);
    }
}

namespace {
std::string g_aniAccessToken;
}  // namespace

void AniClient::setAccessToken(const std::string& token) {
    g_aniAccessToken = token;
}

const char* AniClient::baseUrl() { return kAniBase; }

void AniClient::getTrends(std::function<void(std::vector<SearchSubject>)> cb,
                          ErrorCb error) {
    HTTP::getResult<nlohmann::json>(std::string(kAniBase) + "/v1/trends", {},
        [cb, error](nlohmann::json j) {
#if defined(__SWITCH__)
            aniswitchStartupLog("ANI: trends json enter");
#endif
            std::vector<SearchSubject> out;
            try {
                nlohmann::json arr = nlohmann::json::array();
                if (j.is_object() && j.contains("trendingSubjects") && j["trendingSubjects"].is_array()) {
                    arr = j["trendingSubjects"];
                } else if (j.is_array()) {
                    arr = j;
                }
                for (const auto& it : arr) {
                    SearchSubject s;
                    if (it.contains("bangumiId") && it["bangumiId"].is_number()) {
                        s.id = it["bangumiId"].get<int32_t>();
                    } else if (it.contains("subjectId") && it["subjectId"].is_number()) {
                        s.id = it["subjectId"].get<int32_t>();
                    }
                    if (it.contains("nameCn") && it["nameCn"].is_string()) s.nameCN = it["nameCn"].get<std::string>();
                    if (it.contains("name") && it["name"].is_string()) s.name = it["name"].get<std::string>();
                    if (it.contains("imageLarge") && it["imageLarge"].is_string()) s.images.large = it["imageLarge"].get<std::string>();
                    if (s.id > 0) out.push_back(std::move(s));
                }
            } catch (const std::exception& e) {
                if (error) error(std::string("trends parse: ") + e.what(), -1);
                return;
            }
#if defined(__SWITCH__)
            {
                char _b[80];
                snprintf(_b, sizeof(_b), "ANI: trends parse n=%zu", out.size());
                aniswitchStartupLog(_b);
            }
#endif
            if (cb) cb(std::move(out));
        }, error);
}

namespace {
// Map YYYY-MM-DD to Bangumi weekday 0=Mon .. 6=Sun (home_bangumi uses this).
int weekdayFromYmd(int y, int m, int d) {
    // Sakamoto's algorithm: 0=Sunday .. 6=Saturday, then convert to Mon=0.
    static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    int w = (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;  // 0=Sun
    return (w + 6) % 7;  // 0=Mon
}
int weekdayFromDateString(const std::string& date) {
    int y = 0, m = 0, d = 0;
    if (sscanf(date.c_str(), "%d-%d-%d", &y, &m, &d) == 3) {
        return weekdayFromYmd(y, m, d);
    }
    return 0;
}
}  // namespace

void AniClient::getAiringSchedule(std::function<void(std::vector<CalendarItem>)> cb,
                                  ErrorCb error) {
    // today=YYYY-MM-DD, timeZone=Asia/Shanghai (Switch is typically CN/HK/TW/JP).
    char today[16] = {0};
    {
        std::time_t t = std::time(nullptr);
        std::tm tm{};
#if defined(_WIN32)
        gmtime_s(&tm, &t);
#else
        gmtime_r(&t, &tm);
#endif
        // Use local date roughly via UTC+8 for CN-centric users.
        // Good enough for a schedule grid; not a full timezone engine.
        t += 8 * 3600;
#if defined(_WIN32)
        gmtime_s(&tm, &t);
#else
        gmtime_r(&t, &tm);
#endif
        snprintf(today, sizeof(today), "%04d-%02d-%02d",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    }

    cpr::Parameters params;
    params.Add(cpr::Parameter{"today", today});
    params.Add(cpr::Parameter{"timeZone", "Asia/Shanghai"});

    HTTP::getResult<nlohmann::json>(std::string(kAniBase) + "/v1/schedule/airing", params,
        [cb, error](nlohmann::json j) {
            std::vector<CalendarItem> out;
            try {
                nlohmann::json days = nlohmann::json::array();
                if (j.is_object() && j.contains("list") && j["list"].is_array()) {
                    days = j["list"];
                } else if (j.is_array()) {
                    days = j;
                }
                for (const auto& day : days) {
                    std::string date;
                    if (day.contains("date") && day["date"].is_string()) date = day["date"].get<std::string>();
                    const int wd = weekdayFromDateString(date);
                    if (!day.contains("list") || !day["list"].is_array()) continue;
                    for (const auto& ep : day["list"]) {
                        if (!ep.contains("subject") || !ep["subject"].is_object()) continue;
                        const auto& subj = ep["subject"];
                        CalendarItem item;
                        item.weekday = wd;
                        if (subj.contains("subjectId") && subj["subjectId"].is_number())
                            item.subject.id = subj["subjectId"].get<int32_t>();
                        if (subj.contains("name") && subj["name"].is_string())
                            item.subject.name = subj["name"].get<std::string>();
                        if (subj.contains("nameCn") && subj["nameCn"].is_string())
                            item.subject.nameCN = subj["nameCn"].get<std::string>();
                        if (subj.contains("imageLarge") && subj["imageLarge"].is_string())
                            item.subject.images.large = subj["imageLarge"].get<std::string>();
                        if (item.subject.id > 0) out.push_back(std::move(item));
                    }
                }
            } catch (const std::exception& e) {
                if (error) error(std::string("schedule parse: ") + e.what(), -1);
                return;
            }
            if (cb) cb(std::move(out));
        }, error);
}

void AniClient::searchSubjects(const std::string& query, int limit,
                               std::function<void(std::vector<SearchSubject>)> cb,
                               ErrorCb error) {
    if (query.empty() || limit <= 0) {
        if (cb) cb({});
        return;
    }
    cpr::Parameters params;
    params.Add(cpr::Parameter{"q", query});
    params.Add(cpr::Parameter{"limit", std::to_string(limit)});
    params.Add(cpr::Parameter{"offset", "0"});
    HTTP::getResult<nlohmann::json>(std::string(kAniBase) + "/v2/subjects/search", params,
        [cb, error](nlohmann::json j) {
            std::vector<SearchSubject> out;
            try {
                nlohmann::json arr = nlohmann::json::array();
                if (j.is_object()) {
                    // v21.1: live Ani server returns { "items": [...] }
                    if (j.contains("items") && j["items"].is_array()) arr = j["items"];
                    else if (j.contains("data") && j["data"].is_array()) arr = j["data"];
                    else if (j.contains("list") && j["list"].is_array()) arr = j["list"];
                    else if (j.contains("subjects") && j["subjects"].is_array()) arr = j["subjects"];
                    else if (j.contains("results") && j["results"].is_array()) arr = j["results"];
                } else if (j.is_array()) {
                    arr = j;
                }
                for (const auto& it : arr) {
                    if (!it.is_object()) continue;
                    SearchSubject s;
                    if (it.contains("id") && it["id"].is_number()) s.id = it["id"].get<int32_t>();
                    else if (it.contains("subjectId") && it["subjectId"].is_number()) s.id = it["subjectId"].get<int32_t>();
                    else if (it.contains("bangumiId") && it["bangumiId"].is_number()) s.id = it["bangumiId"].get<int32_t>();
                    if (it.contains("nameCn") && it["nameCn"].is_string()) s.nameCN = it["nameCn"].get<std::string>();
                    if (it.contains("name") && it["name"].is_string()) s.name = it["name"].get<std::string>();
                    if (it.contains("imageLarge") && it["imageLarge"].is_string()) s.images.large = it["imageLarge"].get<std::string>();
                    if (it.contains("images") && it["images"].is_object()) {
                        const auto& img = it["images"];
                        if (img.contains("large") && img["large"].is_string()) s.images.large = img["large"].get<std::string>();
                        if (img.contains("common") && img["common"].is_string()) s.images.common = img["common"].get<std::string>();
                    }
                    if (it.contains("rating") && it["rating"].is_number()) s.score = it["rating"].get<double>();
                    if (s.id > 0) out.push_back(std::move(s));
                }
            } catch (const std::exception& e) {
                if (error) error(std::string("search parse: ") + e.what(), -1);
                return;
            }
            if (cb) cb(std::move(out));
        }, error);
}

}  // namespace aniswitch
