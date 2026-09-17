// SPDX-License-Identifier: AGPL-3.0
//
// Subject "info" fragment: summary + tags + cover. Stub for v0.1.
#pragma once
#include <borealis.hpp>

namespace aniswitch {
class SubjectInfoFragment : public brls::Box {
public:
    explicit SubjectInfoFragment(int32_t subjectId = 0);
private:
    int32_t subjectId_;
};
}
