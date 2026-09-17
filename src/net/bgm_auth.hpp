// SPDX-License-Identifier: AGPL-3.0
//
// Bangumi.tv OAuth 2.0 flow.
//
// Bangumi uses the standard OAuth 2.0 authorization code grant with PKCE
// (since they ship S256 support). Endpoints:
//
//   Authorization: https://bgm.tv/oauth/authorize
//   Token:         https://bgm.tv/oauth/access_token
//
// Required registration parameters:
//   client_id, client_secret (from https://bgm.tv/dev/app)
//   redirect_uri (we use a custom scheme + in-app token paste; the
//                 Switch does not have a system browser, so the user
//                 needs to copy the auth code from a desktop browser
//                 into the app, or we use a QR code that opens the
//                 URL on a phone.)
//   response_type=code
//   scope=
//
// We store the tokens in ProgramConfig::setBangumiToken().
//
// For an app without client_id, Bangumi allows PAT (personal access
// token) bearer headers — that path is also implemented as a fallback
// for one-line auth.

#pragma once

#include <functional>
#include <string>
#include "net/bgm_types.hpp"
#include "net/http.hpp"

namespace aniswitch {

class BangumiAuth {
public:
    struct AuthorizationRequest {
        std::string url;
        std::string codeVerifier;
    };

    // Construct the authorize URL. The user opens this in a desktop
    // browser, logs in, grants the app, and the redirect URL contains
    // ?code=XYZ — they paste the code into the in-app "Auth Code" field.
    static AuthorizationRequest buildAuthorizeUrl(const std::string& clientId,
                                         const std::string& redirectUri,
                                         const std::string& state);

    // Exchange the pasted auth code for an access token.
    static void exchangeCode(const std::string& clientId,
                             const std::string& clientSecret,
                             const std::string& redirectUri,
                             const std::string& code,
                             const std::string& codeVerifier,
                             std::function<void(OAuthToken)> callback = nullptr,
                             ErrorCallback error = nullptr);

    // Refresh an expired access token using the stored refresh token.
    static void refreshAccessToken(const std::string& clientId,
                                   const std::string& clientSecret,
                                   const std::string& redirectUri,
                                   const std::string& refreshToken,
                                   std::function<void(OAuthToken)> callback = nullptr,
                                   ErrorCallback error = nullptr);

    // Same as above but omits redirect_uri.  bgm.tv accepts both; we
    // use this from the background refresh thread on the Switch where
    // the original redirect_uri may not match what the user actually
    // used when generating the token on a desktop (e.g. PC used
    // http://127.0.0.1:8765/callback while the Switch runtime was
    // built for ani-switch://oauth/callback).
    static void refreshAccessTokenNoRedirectUri(const std::string& clientId,
                                                  const std::string& clientSecret,
                                                  const std::string& refreshToken,
                                                  std::function<void(OAuthToken)> callback = nullptr,
                                                  ErrorCallback error = nullptr);

    // PAT fallback: store a user-provided personal access token. We
    // verify it by calling /v0/users/me.
    static void verifyPAT(const std::string& pat,
                          std::function<void(std::string userId)> callback = nullptr,
                          ErrorCallback error = nullptr);

    // Logout (clears local storage; does not revoke on the server).
    static void logout();
};

}  // namespace aniswitch
