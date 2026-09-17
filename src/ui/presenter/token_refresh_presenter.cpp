// SPDX-License-Identifier: AGPL-3.0

#include "ui/presenter/token_refresh_presenter.hpp"
#include "net/bgm_auth.hpp"
#include "utils/config_helper.hpp"
#include <borealis/core/thread.hpp>

namespace aniswitch {

void TokenRefreshPresenter::refreshNow() {
    auto& cfg = ProgramConfig::instance();
    const std::string cid = cfg.getBangumiClientId();
    const std::string csec = cfg.getBangumiClientSecret();
    const std::string rt  = cfg.getBangumiRefreshToken();
    if (cid.empty() || csec.empty() || rt.empty()) {
        onResult.fire("未配置 client_id / client_secret / refresh_token, 无法刷新");
        return;
    }
    BangumiAuth::refreshAccessTokenNoRedirectUri(
        cid, csec, rt,
        uiCallback([this](OAuthToken tok) {
            ProgramConfig::instance().setBangumiToken(
                tok.accessToken, tok.refreshToken,
                tok.expiresIn, tok.userId);
            onResult.fire("OK — token 已刷新, " + std::to_string(tok.expiresIn) + "s 后过期");
        }),
        [this](const std::string& msg, int) {
            onResult.fire("FAIL — " + msg);
        });
}

}  // namespace aniswitch
