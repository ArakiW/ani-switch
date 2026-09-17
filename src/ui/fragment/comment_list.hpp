// SPDX-License-Identifier: AGPL-3.0
//
// Comments fragment. Bangumi /v0/subjects/{id}/comments is not in v0
// of the public API; this fragment is a placeholder for when
// Bangumi ships it.
#pragma once
#include <borealis.hpp>

namespace aniswitch {
class CommentListFragment : public brls::Box {
public:
    explicit CommentListFragment(int32_t subjectId = 0);
private:
    int32_t subjectId_;
};
}
