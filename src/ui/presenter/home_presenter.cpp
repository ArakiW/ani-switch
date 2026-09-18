// SPDX-License-Identifier: AGPL-3.0
#include "ui/presenter/home_presenter.hpp"
#include "net/ani_client.hpp"
#include <borealis/core/logger.hpp>

namespace aniswitch {

void HomePresenter::refresh() {
    // v20.2: Ani-only.  Field log proved api.animeko.org:443
    // connects from Switch without a proxy.  No nested Bangumi
    // fallback on the construction stack.
    // v22.2: HOME LED is playback-scoped — no LED on home rails.
    AniClient::getAiringSchedule(
        uiCallback([this](std::vector<CalendarItem> cal) {
            brls::Logger::info("HomePresenter: ani schedule items={}", cal.size());
            onCalendar.fire(std::move(cal));
        }),
        uiCallback([this](const std::string& msg, int) {
            brls::Logger::warning("HomePresenter: ani schedule failed: {}", msg);
            onCalendar.fire(std::vector<CalendarItem>{});
        }));
}

void HomePresenter::fetchTrending(const std::string& keyword) {
    (void)keyword;
    AniClient::getTrends(
        uiCallback([this](std::vector<SearchSubject> list) {
            brls::Logger::info("HomePresenter: ani trends items={}", list.size());
            onTrending.fire(std::move(list));
        }),
        uiCallback([this](const std::string& msg, int) {
            brls::Logger::warning("HomePresenter: ani trends failed: {}", msg);
            onTrending.fire(std::vector<SearchSubject>{});
        }));
}

}  // namespace aniswitch
