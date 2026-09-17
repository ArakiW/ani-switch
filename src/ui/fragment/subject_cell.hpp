// SPDX-License-Identifier: AGPL-3.0
//
// SubjectCell — reusable card showing a Bangumi subject with its
// cover image, title, summary line (rating + episode count), and
// an optional Chinese / Japanese title under the main one.
//
// Used in: search results, trending, schedule, my collection,
// history, and the subject detail page.  Clicking the cell fires
// the user-supplied `onClick` callback with the Bangumi subject
// id, so the parent activity can navigate to the subject detail
// page.

#pragma once

#include <borealis.hpp>
#include <functional>
#include <string>
#include "net/bgm_types.hpp"

namespace aniswitch {

class SubjectCell : public brls::Box {
public:
    SubjectCell();

    // Bind a subject to this cell.  Safe to call from any thread
    // (UI must be on the main thread).  Cover download is gated by
    // theme::kCoverEnabled (v22 P0 unified switch).
    void setSubject(const Subject& s, const std::string& imageUrl = "");

    // Click handler.  Called with the subject's Bangumi id.
    void setOnClick(std::function<void(int32_t)> onClick);

    void onFocusGained() override;
    void onFocusLost() override;

    // Sizing helpers used by the recycling grid to keep rows
    // consistent.
    static constexpr int COVER_WIDTH  = 56;
    static constexpr int COVER_HEIGHT = 84;

private:
    brls::Image* cover_      = nullptr;
    brls::Label* title_      = nullptr;
    brls::Label* subtitle_   = nullptr;
    brls::Label* meta_       = nullptr;
    int32_t subjectId_       = 0;
    std::function<void(int32_t)> onClick_;
};

}  // namespace aniswitch
