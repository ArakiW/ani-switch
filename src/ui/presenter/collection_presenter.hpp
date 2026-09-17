// SPDX-License-Identifier: AGPL-3.0
//
// My-collection presenter. Wraps CollectionManager for the activity.

#pragma once

#include "ui/presenter/presenter.hpp"
#include "utils/sqlite_store.hpp"
#include <borealis/core/event.hpp>
#include <vector>

namespace aniswitch {

class CollectionPresenter : public Presenter {
public:
    brls::Event<std::vector<SQLiteStore::CollectionEntry>> onCollection;
    brls::Event<std::string>                               onError;

    void setType(int32_t type) { type_ = type; }
    void refreshFromRemote();
    void refreshFromLocal();

private:
    int32_t type_ = 0;
};

// v18.9: Single-subject collection editor.  Used by
// SubjectActivity to let the user flip a subject between
// "想看 / 在看 / 看过 / 搁置 / 抛弃" and back.  Bangumi's
// API treats 1-5 as the five user collection types; we
// forward the new type via PATCH /v0/users/-/collections/{id}.
class SingleCollectionEditor : public Presenter {
public:
    // (newType, epStatus) — the freshly persisted values.  -1
    // means "deleted from collection".
    brls::Event<int32_t> onUpdated;

    // subjectId is the Bangumi subject id.  type ∈ 1..5.
    // Pass type=0 to remove the subject from the user's
    // collection.
    void setCollectionType(int32_t subjectId, int32_t type);
};

}  // namespace aniswitch
