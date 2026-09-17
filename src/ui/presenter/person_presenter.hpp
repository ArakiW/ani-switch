// SPDX-License-Identifier: AGPL-3.0
//
// Presenter for the person detail screen.  Pulls the person
// record + their filmography in two parallel cpr calls.
#pragma once
#include "ui/presenter/presenter.hpp"
#include "net/bgm_types.hpp"
#include <borealis/core/event.hpp>
#include <vector>

namespace aniswitch {

class PersonPresenter : public Presenter {
public:
    brls::Event<Person>                       onPerson;
    brls::Event<std::vector<SearchSubject>>   onSubjects;

    void setPersonId(int32_t id) { personId_ = id; }
    void refresh();

private:
    int32_t personId_ = 0;
};

}  // namespace aniswitch
