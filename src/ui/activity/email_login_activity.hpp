// SPDX-License-Identifier: AGPL-3.0
//
// v18.5: animeko 6.1.0 EmailLoginStartScreen + EmailLoginVerifyScreen
// ported as a single activity (the two screens share most of the
// layout, and the Switch memory is tight).  Step 1 = "enter email
// + send OTP", step 2 = "enter 6-digit code + finish".
//
// We use ani server's email-OTP path, which hands us both an ani
// JWT and an optional Bangumi PAT in one round-trip, so a single
// login flow replaces what used to be two separate authorisations.

#pragma once

#include <borealis.hpp>
#include "net/ani_client.hpp"
#include <string>

namespace aniswitch {

class EmailLoginActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/email_login_activity.xml");
    EmailLoginActivity();
    void onContentAvailable() override;
private:
    void renderStart();
    void renderVerify(const std::string& otpId,
                      const std::string& email,
                      bool hasExistingUser);

    brls::Box*   root_         = nullptr;
    brls::Label* statusLabel_  = nullptr;

    std::string pendingEmail_;
    std::string pendingOtpId_;
    bool        pendingHasExistingUser_ = false;
};

}  // namespace aniswitch
