// SPDX-License-Identifier: AGPL-3.0
//
// Person (voice actor / staff) detail page.  Shows the person's
// avatar, name (CN + original), short bio, career tags, and a
// scrollable list of subjects they appeared in.
//
// Reached by tapping a cast chip in subject detail (SubjectActivity
// onCharacters → Intent::openPerson).
#pragma once
#include <borealis.hpp>
#include "ui/presenter/person_presenter.hpp"
#include "net/bgm_types.hpp"
#include <vector>

namespace aniswitch {
class PersonActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/person_activity.xml");
    explicit PersonActivity(int32_t personId);
    ~PersonActivity() override;
    void onContentAvailable() override;
private:
    void onPerson(Person p);
    void onSubjects(std::vector<SearchSubject> s);

    int32_t personId_ = 0;
    PersonPresenter presenter_;

    brls::Image* avatar_    = nullptr;
    brls::Label* name_      = nullptr;
    brls::Label* altName_   = nullptr;
    brls::Label* type_      = nullptr;
    brls::Box*   careerBox_ = nullptr;
    brls::Box*   list_      = nullptr;
};
}
