// SPDX-License-Identifier: AGPL-3.0
#include "ui/token_refresh_task.hpp"
#include "net/bgm_auth.hpp"
#include "net/bgm_client.hpp"
#include "net/myani_client.hpp"
#include "utils/config_helper.hpp"
#include <borealis/core/logger.hpp>
#include <chrono>
#include <ctime>

namespace aniswitch {

namespace {
constexpr int kOneHourMs   = 60 * 60 * 1000;
constexpr int kOneDaySec   = 24 * 60 * 60;
}  // namespace

TokenRefreshTask::TokenRefreshTask()
    : brls::RepeatingTask(kOneHourMs) {
    // brls::RepeatingTask only takes a period; we have to call
    // start() from main.cpp once the borealis Application is up.
}

void TokenRefreshTask::run() {
    if (stopped_.load()) return;
    try {
        auto& cfg = ProgramConfig::instance();
        const std::string rt = cfg.getBangumiRefreshToken();
        if (rt.empty()) return;
        const int64_t expiry = cfg.getBangumiTokenExpiry();
        const int64_t now    = static_cast<int64_t>(std::time(nullptr));
        if (expiry == 0 || expiry - now > kOneDaySec) {
            // Still has >24h of life; skip the round trip.
            return;
        }
        brls::Logger::info("TokenRefreshTask: expires in {}s, refreshing...", expiry - now);
        BangumiAuth::refreshAccessTokenNoRedirectUri(
            cfg.getBangumiClientId(), cfg.getBangumiClientSecret(), rt,
            [&cfg](OAuthToken t) {
                cfg.setBangumiToken(t.accessToken, t.refreshToken, t.expiresIn, t.userId);
                BangumiClient::setAccessToken(t.accessToken);
                MyaniClient::instance().setBangumiToken(t.accessToken);
                brls::Logger::info("TokenRefreshTask: refreshed, +{}s", t.expiresIn);
            },
            [](const std::string& msg, int code) {
                brls::Logger::warning("TokenRefreshTask: refresh failed [{}]: {}", code, msg);
            });
    } catch (const std::exception& e) {
        brls::Logger::warning("TokenRefreshTask: {}", e.what());
    } catch (...) {
        brls::Logger::warning("TokenRefreshTask: unknown exception");
    }
}

void TokenRefreshTask::stop() {
    stopped_.store(true);
    brls::RepeatingTask::stop();
}

}  // namespace aniswitch
