// SPDX-License-Identifier: AGPL-3.0
#include "ui/presenter/subject_presenter.hpp"
#include "net/bgm_client.hpp"
#include <borealis/core/logger.hpp>

namespace aniswitch {

void SubjectPresenter::setSubjectId(int32_t id) { subjectId_ = id; }

void SubjectPresenter::refresh() {
    if (subjectId_ == 0) return;
    const int32_t id = subjectId_;
    auto error = [this, id](const std::string& message, int) {
        brls::Logger::warning("subject: {}", message);
        // v22: surface subject-level failure so the activity can
        // fall back to the demo shell instead of a gray page.
        uiCallback([this, id](const std::string& msg) {
            if (id == subjectId_) onSubjectError.fire(msg);
        })(message);
    };
    BangumiClient::getSubject(id,
        uiCallback([this, id](Subject s) { if (id == subjectId_) onSubject.fire(std::move(s)); }), error);
    BangumiClient::getEpisodes(id, 0,
        uiCallback([this, id](std::vector<Episode> e) { if (id == subjectId_) onEpisodes.fire(std::move(e)); }), error);
    BangumiClient::getCharacters(id,
        uiCallback([this, id](std::vector<SubjectCharacter> c) { if (id == subjectId_) onCharacters.fire(std::move(c)); }), error);
    BangumiClient::getRelations(id,
        uiCallback([this, id](std::vector<SubjectRelation> r) { if (id == subjectId_) onRelations.fire(std::move(r)); }), error);
    BangumiClient::getSubjectComments(id, 20, 0,
        uiCallback([this, id](std::vector<Comment> c) { if (id == subjectId_) onComments.fire(std::move(c)); }), error);
    // v17.2: rating (public) + my-collection status (auth).
    // Both are best-effort and fire as separate events so the
    // activity can render them even if one fails (e.g. 404
    // for not-yet-rated or not-in-collection).
    BangumiClient::getSubjectRating(id,
        uiCallback([this, id](SubjectRating r) { if (id == subjectId_) onRating.fire(std::move(r)); }), error);
    BangumiClient::getMyCollectionStatus(id,
        uiCallback([this, id](UserCollection c) { if (id == subjectId_) onMyCollection.fire(std::move(c)); }), error);
}

}  // namespace aniswitch
