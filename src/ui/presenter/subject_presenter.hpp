// SPDX-License-Identifier: AGPL-3.0
//
// Subject detail presenter. Pulls the Subject + episodes + characters
// + relations for a given subject id. Exposes them as separate events
// so the activity can render them in tabs.

#pragma once

#include "ui/presenter/presenter.hpp"
#include "net/bgm_types.hpp"
#include <borealis/core/event.hpp>

namespace aniswitch {

class SubjectPresenter : public Presenter {
public:
    brls::Event<Subject>                       onSubject;
    brls::Event<std::vector<Episode>>          onEpisodes;
    brls::Event<std::vector<SubjectCharacter>> onCharacters;
    brls::Event<std::vector<SubjectRelation>>  onRelations;
    brls::Event<std::vector<Comment>>          onComments;
    brls::Event<SubjectRating>                 onRating;       // v17.2
    brls::Event<UserCollection>                onMyCollection; // v17.2
    // v22: subject fetch failed — activity renders demo shell.
    brls::Event<std::string>                   onSubjectError;

    void setSubjectId(int32_t id);
    void refresh();

private:
    int32_t subjectId_ = 0;
};

}  // namespace aniswitch
