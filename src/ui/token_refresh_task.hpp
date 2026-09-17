// SPDX-License-Identifier: AGPL-3.0
//
// Periodic Bangumi OAuth refresh, executed on the borealis main
// thread via brls::RepeatingTask.  v16.9 used std::thread +
// std::this_thread::sleep_for + detach; on Switch newlib that
// can std::terminate the NRO (pthread limit / std::chrono
// differences / unknown — the v15.3 outer+inner try/catch
// couldn't help because no exception was raised, the process
// was just killed).
//
// brls::RepeatingTask instead piggybacks on the main loop's
// Timer list.  Each tick is dispatched on the UI thread, no
// extra thread is created.

#pragma once
#include <borealis/core/task.hpp>
#include <atomic>

namespace aniswitch {

class TokenRefreshTask : public brls::RepeatingTask {
public:
    TokenRefreshTask();

    // The main-loop callback.  Cheap, no network — we just
    // check whether the access token expires soon and, if so,
    // run refreshAccessTokenNoRedirectUri.
    void run() override;

    // Stop the loop and unsubscribe.
    void stop();

private:
    std::atomic<bool> stopped_{false};
};

}  // namespace aniswitch
