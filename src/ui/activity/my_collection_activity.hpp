// SPDX-License-Identifier: AGPL-3.0
//
// My collection. v22: 5-state filter chips + 6-col poster grid.

#pragma once

#include <borealis.hpp>
#include "ui/presenter/collection_presenter.hpp"

namespace aniswitch {

class MyCollectionActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/my_collection_activity.xml");
    MyCollectionActivity();
    void onContentAvailable() override;
    void onResume() override;

private:
    CollectionPresenter presenter_;
    brls::Box* list_ = nullptr;
    brls::Box* filterRow_ = nullptr;
    int32_t filterType_ = -1;  // -1 = all
    brls::View* lastFocused_ = nullptr;
    std::vector<SQLiteStore::CollectionEntry> cache_;
    void renderFilterChips();
    void render(const std::vector<SQLiteStore::CollectionEntry>& v);
};

}  // namespace aniswitch
