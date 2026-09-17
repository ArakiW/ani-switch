// SPDX-License-Identifier: AGPL-3.0
#pragma once
#include <borealis.hpp>
#include <vector>
#include "ui/presenter/subject_presenter.hpp"
#include "ui/presenter/collection_presenter.hpp"  // v18.9: 5-state flip
namespace aniswitch {
class SubjectActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/subject_activity.xml");
    SubjectActivity(int32_t subjectId, const std::string& type = "anime");
    ~SubjectActivity() override;
    void onContentAvailable() override;
private:
    void onSubject(Subject s);
    void onEpisodes(std::vector<Episode> e);
    void onCharacters(std::vector<SubjectCharacter> c);
    void onRelations(std::vector<SubjectRelation> r);  // v17.1
    void onComments(std::vector<Comment> v);
    void onRating(SubjectRating r);                     // v17.2
    void onMyCollection(UserCollection c);              // v17.2

    int32_t subjectId_      = 0;
    Subject subject_;
    SubjectPresenter presenter_;
    SingleCollectionEditor editor_;  // v18.9

    brls::Image* cover_      = nullptr;
    brls::Label* title_      = nullptr;
    brls::Label* altTitle_   = nullptr;
    brls::Label* rating_     = nullptr;
    brls::Label* meta_       = nullptr;
    brls::Box*   tagsBox_    = nullptr;
    brls::Label* summary_    = nullptr;
    brls::Box*   episodeList_ = nullptr;
    // v22: contentView is the HUD wrap Box; this is the real scroll.
    brls::ScrollingFrame* scroll_ = nullptr;
};
}
