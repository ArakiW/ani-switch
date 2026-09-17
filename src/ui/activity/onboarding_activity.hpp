// SPDX-License-Identifier: AGPL-3.0
//
// v17.3: First-run onboarding activity.  Shown the very first time
// ani-switch boots (when ProgramConfig::isFirstRun() returns true)
// and lets the user walk through 5 short screens that introduce
// the major features.  Once the user reaches the final screen and
// presses "进入应用", the activity calls
// ProgramConfig::markFirstRunDone() and pops itself, revealing
// MainActivity underneath.  Reachable later via
// Intent::openOnboarding() — settings has a "重新走引导" entry.

#pragma once

#include <borealis.hpp>

namespace aniswitch {

class OnboardingActivity : public brls::Activity {
public:
    // Required: setContentView(nullptr) null-derefs without a content view.
    CONTENT_FROM_XML_RES("activity/onboarding_activity.xml");
    OnboardingActivity();
    void onContentAvailable() override;

private:
    // total step count; bump this when adding a screen
    static constexpr int kStepCount = 5;

    int currentStep_ = 0;

    // Re-render the content box to match currentStep_.  The
    // previous step's widgets are deleted by replacing the
    // contentView pointer (brls::Activity owns it).
    void renderStep();

    // Helpers for the individual step builders — each takes the
    // root column and appends its widgets.
    void buildStepWelcome(brls::Box* root);
    void buildStepTheme(brls::Box* root);
    void buildStepLogin(brls::Box* root);
    void buildStepDataSources(brls::Box* root);
    void buildStepDone(brls::Box* root);

    // Footer button row: < 上一步 | 进度点 | 下一步 >.  The
    // "下一步" label is "完成" on the last step and triggers
    // markFirstRunDone + openMain.
    void buildFooter(brls::Box* root);
};

}  // namespace aniswitch
