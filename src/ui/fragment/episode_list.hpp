// SPDX-License-Identifier: AGPL-3.0
//
// Episode list fragment. Used as a tab inside SubjectActivity. The
// full data is already rendered by SubjectActivity in v0.1; this
// fragment exists so a future tabbed refactor can use it.
#pragma once
#include <borealis.hpp>

namespace aniswitch {
class EpisodeListFragment : public brls::Box {
public:
    explicit EpisodeListFragment(int32_t subjectId = 0);
private:
    int32_t subjectId_;
};
}
