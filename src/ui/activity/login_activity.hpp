// SPDX-License-Identifier: AGPL-3.0
//
// Login activity. Bangumi OAuth 2.0 (the user copies the code from a
// desktop browser) or PAT (one-line personal access token). The
// latter is recommended because the Switch has no system browser.

#pragma once

#include <borealis.hpp>
#include <memory>

namespace aniswitch {

class LoginActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/login_activity.xml");
    LoginActivity();
    void onContentAvailable() override;
private:
    std::shared_ptr<int> lifetime_ = std::make_shared<int>(0);
    brls::Label* status_ = nullptr;
    std::string statusText_;
};

}  // namespace aniswitch
