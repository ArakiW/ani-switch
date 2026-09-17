// SPDX-License-Identifier: AGPL-3.0
//
// Subject reviews (长评) fragment. Stub.
#pragma once
#include <borealis.hpp>

namespace aniswitch {
class SeasonEvaluateFragment : public brls::Box {
public:
    explicit SeasonEvaluateFragment(int32_t subjectId = 0);
private:
    int32_t subjectId_;
};
}
