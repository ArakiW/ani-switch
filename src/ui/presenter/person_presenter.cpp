// SPDX-License-Identifier: AGPL-3.0
#include "ui/presenter/person_presenter.hpp"
#include "net/bgm_client.hpp"
#include <borealis/core/logger.hpp>

namespace aniswitch {

void PersonPresenter::refresh() {
    if (personId_ <= 0) return;
    const int32_t id = personId_;
    auto error = [](const std::string& message, int) {
        brls::Logger::warning("person: {}", message);
    };
    BangumiClient::getPerson(id,
        uiCallback([this, id](Person p) { if (id == personId_) onPerson.fire(std::move(p)); }),
        error);
    BangumiClient::getPersonSubjects(id, 30,
        uiCallback([this, id](std::vector<SearchSubject> s) {
            if (id == personId_) onSubjects.fire(std::move(s));
        }),
        error);
}

}  // namespace aniswitch
