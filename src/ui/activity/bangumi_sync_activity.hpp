// SPDX-License-Identifier: AGPL-3.0
//
// v18.4: BangumiSyncActivity — BangumiSyncTab from animeko 6.1.0
// ported to borealis.  Shows:
//   * whether the access token is set
//   * user_id + token expiry
//   * "立即刷新" button (manual refresh)
//   * "退出登录" button
//   * last refresh status / time
//
// Pure-local UI; all network goes through existing
// BangumiAuth::refreshAccessTokenNoRedirectUri (no new HTTP
// paths).

#pragma once

#include <borealis.hpp>
#include "ui/presenter/token_refresh_presenter.hpp"

namespace aniswitch {

class BangumiSyncActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/bangumi_sync_activity.xml");
    BangumiSyncActivity();
    void onContentAvailable() override;
private:
    void renderState();
    brls::Label* statusLabel_ = nullptr;
    brls::Label* detailLabel_  = nullptr;
    TokenRefreshPresenter presenter_;
};

}  // namespace aniswitch
