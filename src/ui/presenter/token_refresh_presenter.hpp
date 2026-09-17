// SPDX-License-Identifier: AGPL-3.0
//
// v18.4: TokenRefreshPresenter for the manual "立即刷新" button
// on BangumiSyncActivity.  Wires the existing
// BangumiAuth::refreshAccessTokenNoRedirectUri to a UI callback
// that BangumiSyncActivity subscribes to.

#pragma once

#include <borealis.hpp>
#include "ui/presenter/presenter.hpp"

namespace aniswitch {

class TokenRefreshPresenter : public Presenter {
public:
    brls::Event<std::string> onResult;   // human-readable status

    void refreshNow();
};

}  // namespace aniswitch
