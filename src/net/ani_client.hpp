// SPDX-License-Identifier: AGPL-3.0
//
// v18.5: ani server (api.animeko.org) email-OTP login client.
// animeko 6.1.0's primary login path is /v2/users/auth/email/*,
// not Bangumi OAuth — the ani server hands us a Bangumi PAT
// alongside its own JWT, so a single email login covers both
// systems.
//
// Endpoints (OpenAPI-generated paths from animeko 6.1.0
// client/src/commonMain/.../apis/UserAuthenticationAniApi.kt):
//   POST /v2/users/auth/email/otp
//     body: { email: string, language: AniEmailLanguage? }
//     resp: { otpId: string, hasExistingUser: bool }
//   POST /v2/users/auth/email
//     body: { otpId: string, otpValue: string }
//     resp: { userId, tokens: { accessToken, refreshToken,
//                                 expiresAtMillis,
//                                 bangumiAccessToken? },
//              user }

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "net/bgm_types.hpp"
#include "net/http.hpp"
#include "net/json_helper.hpp"

namespace aniswitch {

struct AniOtpResponse {
    std::string otpId;
    bool        hasExistingUser = false;
};

struct AniTokens {
    std::string accessToken;
    std::string refreshToken;
    int64_t     expiresAtMillis = 0;
    std::string bangumiPat;  // optional
};

struct AniLoginResponse {
    std::string userId;
    AniTokens   tokens;
};

class AniClient {
public:
    using OtpCb       = std::function<void(AniOtpResponse)>;
    using LoginCb     = std::function<void(AniLoginResponse)>;
    using ErrorCb     = ErrorCallback;

    // Step 1: ask ani server to email a 6-digit OTP.  Fires the
    // otpId back via callback on success.
    static void sendEmailOtp(const std::string& email,
                             OtpCb callback = nullptr,
                             ErrorCb error = nullptr);

    // Step 2: present the OTP code.  On success the callback
    // receives the AniTokens (ani JWT + optional Bangumi PAT).
    static void loginByEmailOtp(const std::string& otpId,
                                const std::string& otpValue,
                                LoginCb callback = nullptr,
                                ErrorCb error = nullptr);

    // Wires the ani JWT into cpr's session creation.  Called
    // by EmailLoginActivity after a successful login.  Resets
    // to "" on logout.
    static void setAccessToken(const std::string& token);

    // ---- v20.0 data plane (animeko 6.1) --------------------------------
    // GET /v1/trends — Bangumi-subject hot list cached by Ani server.
    static void getTrends(std::function<void(std::vector<SearchSubject>)> cb,
                          ErrorCb error = nullptr);

    // GET /v1/schedule/airing?today=YYYY-MM-DD&timeZone=Asia/Shanghai
    // Mapped into CalendarItem{weekday, subject} for the existing
    // HomeBangumiFragment.
    static void getAiringSchedule(std::function<void(std::vector<CalendarItem>)> cb,
                                  ErrorCb error = nullptr);

    // GET /v2/subjects/search?q=&limit= — Ani search (may require
    // auth on some deployments; we try anonymously first).
    static void searchSubjects(const std::string& query, int limit,
                               std::function<void(std::vector<SearchSubject>)> cb,
                               ErrorCb error = nullptr);

    static const char* baseUrl();
};

}  // namespace aniswitch
