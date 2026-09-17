// SPDX-License-Identifier: AGPL-3.0

#include "net/bgm_auth.hpp"
#include "net/bgm_client.hpp"
#include "utils/config_helper.hpp"
#include <cpr/cpr.h>
#include <borealis/core/logger.hpp>
#include <fmt/format.h>
#include <mbedtls/sha256.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <cstring>
#include <stdexcept>
#include <cpr/util.h>

namespace aniswitch {

namespace {
    std::string base64Url(const unsigned char* data, size_t len) {
        // Naive base64url-encode. Avoids pulling in an extra dep.
        static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        std::string out;
        out.reserve(((len + 2) / 3) * 4);
        for (size_t i = 0; i < len; i += 3) {
            uint32_t v = (uint32_t)data[i] << 16;
            if (i + 1 < len) v |= (uint32_t)data[i+1] << 8;
            if (i + 2 < len) v |= (uint32_t)data[i+2];
            out.push_back(tbl[(v >> 18) & 0x3f]);
            out.push_back(tbl[(v >> 12) & 0x3f]);
            if (i + 1 < len) out.push_back(tbl[(v >>  6) & 0x3f]);
            if (i + 2 < len) out.push_back(tbl[(v      ) & 0x3f]);
        }
        return out;
    }

    // PKCE: 32 cryptographically random bytes, base64url-encoded.
    // Switch has mbedTLS, not OpenSSL. mbedtls_ctr_drbg gives us the
    // equivalent of RAND_bytes() �?AES-256-CTR_DRBG seeded from an
    // entropy source (which on Switch comes from getrandom() / hardware).
    std::string genPKCE() {
        unsigned char buf[32] = {0};
        mbedtls_entropy_context entropy;
        mbedtls_ctr_drbg_context ctr_drbg;
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&ctr_drbg);
        int result = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
            reinterpret_cast<const unsigned char*>("aniswitch_pkce"), 14);
        if (result == 0) result = mbedtls_ctr_drbg_random(&ctr_drbg, buf, sizeof(buf));
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        if (result != 0) throw std::runtime_error("PKCE random generation failed");
        return base64Url(buf, sizeof(buf));
    }

    std::string s256(const std::string& in) {
        unsigned char digest[32];   // SHA-256 is always 32 bytes
        if (mbedtls_sha256_ret(reinterpret_cast<const unsigned char*>(in.data()), in.size(),
                           digest, /*is224=*/0) != 0)
            throw std::runtime_error("PKCE SHA-256 failed");
        return base64Url(digest, sizeof(digest));
    }
}  // namespace

BangumiAuth::AuthorizationRequest BangumiAuth::buildAuthorizeUrl(const std::string& clientId,
                                           const std::string& redirectUri,
                                           const std::string& state) {
    std::string codeVerifier = genPKCE();
    std::string challenge    = s256(codeVerifier);

    return {fmt::format(
        "https://bgm.tv/oauth/authorize"
        "?response_type=code"
        "&client_id={}"
        "&redirect_uri={}"
        "&code_challenge={}"
        "&code_challenge_method=S256"
        "&state={}",
        cpr::util::urlEncode(clientId), cpr::util::urlEncode(redirectUri), challenge,
        cpr::util::urlEncode(state)), codeVerifier};
}

void BangumiAuth::exchangeCode(const std::string& clientId,
                               const std::string& clientSecret,
                               const std::string& redirectUri,
                               const std::string& code,
                               const std::string& codeVerifier,
                               std::function<void(OAuthToken)> callback, ErrorCallback error) {
    cpr::Payload payload = {
        {"grant_type",    "authorization_code"},
        {"client_id",     clientId},
        {"client_secret", clientSecret},
        {"redirect_uri",  redirectUri},
        {"code",          code},
        {"code_verifier", codeVerifier},
    };
    auto session = HTTP::createSession();
    // v22: keep hostname URL + SetResolve (SNI).
    const std::string tokenUrl = "https://bgm.tv/oauth/access_token";
    HTTP::applyDnsToSession(*session, tokenUrl);
    session->SetUrl(cpr::Url{tokenUrl});
    session->SetPayload(payload);
    session->PostCallback([callback, err = std::move(error)](const cpr::Response& r) {
        if (r.error) { fireError(err, r.error.message, -1); return; }
        if (r.status_code != 200) {
            fireError(err, fmt::format("HTTP {}", r.status_code), r.status_code);
            return;
        }
        try {
            auto j = nlohmann::json::parse(r.text);
            OAuthToken t;
            t.accessToken  = j.value("access_token",  "");
            t.refreshToken = j.value("refresh_token", "");
            t.tokenType    = j.value("token_type",    "Bearer");
            t.expiresIn    = j.value("expires_in",    0LL);
            t.scope        = j.value("scope",         "");
            t.obtainedAt   = static_cast<int64_t>(time(nullptr));

            // Bangumi's token response also returns the user_id under
            // "user_id" (a number) when grant_type=password, but for
            // the authorization_code path we may need a second /me call.
            // For now we just persist what we have.
            if (j.contains("user_id") && j["user_id"].is_number_integer()) {
                t.userId = std::to_string(j["user_id"].get<int64_t>());
            }
            if (callback) callback(std::move(t));
        } catch (const std::exception& e) {
            fireError(err, e.what(), 200);
        }
    });
}

void BangumiAuth::refreshAccessToken(const std::string& clientId,
                                    const std::string& clientSecret,
                                    const std::string& redirectUri,
                                    const std::string& refreshToken,
                                    std::function<void(OAuthToken)> callback, ErrorCallback error) {
    cpr::Payload payload = {
        {"grant_type",    "refresh_token"},
        {"client_id",     clientId},
        {"client_secret", clientSecret},
        {"redirect_uri",  redirectUri},
        {"refresh_token", refreshToken},
    };
    auto session = HTTP::createSession();
    // v22: keep hostname URL + SetResolve (SNI).
    const std::string tokenUrl = "https://bgm.tv/oauth/access_token";
    HTTP::applyDnsToSession(*session, tokenUrl);
    session->SetUrl(cpr::Url{tokenUrl});
    session->SetPayload(payload);
    session->PostCallback([callback, err = std::move(error)](const cpr::Response& r) {
        if (r.error) { fireError(err, r.error.message, -1); return; }
        if (r.status_code != 200) {
            fireError(err, fmt::format("HTTP {}", r.status_code), r.status_code);
            return;
        }
        try {
            auto j = nlohmann::json::parse(r.text);
            OAuthToken t;
            t.accessToken  = j.value("access_token",  "");
            t.refreshToken = j.value("refresh_token", "");
            t.tokenType    = j.value("token_type",    "Bearer");
            t.expiresIn    = j.value("expires_in",    0LL);
            t.scope        = j.value("scope",         "");
            t.obtainedAt   = static_cast<int64_t>(time(nullptr));
            if (callback) callback(std::move(t));
        } catch (const std::exception& e) {
            fireError(err, e.what(), 200);
        }
    });
}

// refresh grant: the OAuth 2.0 RFC says redirect_uri must match the
// one used during authorization, but bgm.tv only requires the
// grant_type + client_id + client_secret + refresh_token.  We send
// the redirect_uri anyway as a courtesy, falling back to a request
// without it on the first 400.
void BangumiAuth::refreshAccessTokenNoRedirectUri(const std::string& clientId,
                                                  const std::string& clientSecret,
                                                  const std::string& refreshToken,
                                                  std::function<void(OAuthToken)> callback, ErrorCallback error) {
    cpr::Payload payload = {
        {"grant_type",    "refresh_token"},
        {"client_id",     clientId},
        {"client_secret", clientSecret},
        {"refresh_token", refreshToken},
    };
    auto session = HTTP::createSession();
    // v22: keep hostname URL + SetResolve (SNI).
    const std::string tokenUrl = "https://bgm.tv/oauth/access_token";
    HTTP::applyDnsToSession(*session, tokenUrl);
    session->SetUrl(cpr::Url{tokenUrl});
    session->SetPayload(payload);
    session->PostCallback([callback, err = std::move(error)](const cpr::Response& r) {
        if (r.error) { fireError(err, r.error.message, -1); return; }
        if (r.status_code != 200) {
            fireError(err, fmt::format("HTTP {}", r.status_code), r.status_code);
            return;
        }
        try {
            auto j = nlohmann::json::parse(r.text);
            OAuthToken t;
            t.accessToken  = j.value("access_token",  "");
            t.refreshToken = j.value("refresh_token", "");
            t.tokenType    = j.value("token_type",    "Bearer");
            t.expiresIn    = j.value("expires_in",    0LL);
            t.scope        = j.value("scope",         "");
            t.obtainedAt   = static_cast<int64_t>(time(nullptr));
            if (callback) callback(std::move(t));
        } catch (const std::exception& e) {
            fireError(err, e.what(), 200);
        }
    });
}

void BangumiAuth::verifyPAT(const std::string& pat,
                            std::function<void(std::string)> callback, ErrorCallback error) {
    auto session = HTTP::createSession();
    // v22: hostname URL + SetResolve (SNI).
    const std::string meUrl = BangumiClient::baseUrl() + "/v0/me";
    HTTP::applyDnsToSession(*session, meUrl);
    session->SetUrl(cpr::Url{meUrl});
    // v16.10.8.1: include Accept in the same SetHeader call so
    // the User-Agent / Accept installed by createSession()
    // aren't replaced away.  (cpr SetHeader is `header_ = ...`).
    session->SetHeader({
        {"User-Agent",   "ani-switch/0.1.0"},
        {"Accept",       "application/json"},
        {"Authorization","Bearer " + pat},
    });
    session->GetCallback([callback, err = std::move(error)](const cpr::Response& r) {
        if (r.error) { fireError(err, r.error.message, -1); return; }
        if (r.status_code != 200) {
            fireError(err, fmt::format("HTTP {}", r.status_code), r.status_code);
            return;
        }
        try {
            auto j = nlohmann::json::parse(r.text);
            std::string id = std::to_string(j.value("id", 0LL));
            if (callback) callback(std::move(id));
        } catch (const std::exception& e) {
            fireError(err, e.what(), 200);
        }
    });
}

void BangumiAuth::logout() {
    ProgramConfig::instance().logout();
    BangumiClient::setAccessToken("");
}

}  // namespace aniswitch
