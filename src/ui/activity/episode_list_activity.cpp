// SPDX-License-Identifier: AGPL-3.0
//
// v22 chrome: title kTypeH2, 88px list rows via chrome::makeListRow,
// scroll shell + HUD + B 返回. openSourcePicker click logic unchanged.

#include "ui/activity/episode_list_activity.hpp"
#include "ui/ui_chrome.hpp"
#include "utils/activity_helper.hpp"
#include "utils/string_helper.hpp"
#include "utils/sqlite_store.hpp"
#include <borealis/core/logger.hpp>
#include <fmt/format.h>
#include <borealis/views/scrolling_frame.hpp>

namespace aniswitch {

EpisodeListActivity::EpisodeListActivity(int32_t subjectId, int32_t resumeEpisodeId)
    : subjectId_(subjectId), resumeEpisodeId_(resumeEpisodeId) {}

void EpisodeListActivity::onContentAvailable() {
    // Padded chrome page as scroll content (replaces XML placeholder).
    auto* content = chrome::makePageRoot();

    content->addView(chrome::makeTitle("剧集列表", theme::kTypeH2, 8));
    content->addView(chrome::makeMuted("选择一集 · A 打开播放源 · B 返回", 16));

    auto* list = new brls::Box();
    list->setAxis(brls::Axis::COLUMN);
    list->setId("eps_list");
    content->addView(list);
    list_ = list;

    // HUD carries B 返回 — drop the ad-hoc footer button.
    auto* shell = chrome::attachScrollShell(this, content);
    if (shell) shell->setBackgroundColor(theme::kChromeBg);
    chrome::registerBack(this);

    presenter_.onEpisodes.subscribe([this](std::vector<Episode> e) { render(e); });
    presenter_.onLastWatched.subscribe([this, list](std::optional<SQLiteStore::HistoryEntry> last) {
        if (!last) return;
        // If we have a resume hint, jump straight in after the list
        // populates. The episode tap is the user-confirmable path.
        (void)list;
    });
    presenter_.setSubjectId(subjectId_);
    presenter_.refresh();

    chrome::finish(this, shell);
}

void EpisodeListActivity::render(const std::vector<Episode>& eps) {
    if (!list_) return;
    list_->clearViews();
    for (const auto& e : eps) {
        auto* row = chrome::makeListRow(theme::kRowHeight);  // 88 · kDarkCardRowBg

        auto* idx = new brls::Label();
        idx->setText(fmt::format("EP{:g}", e.sort));
        idx->setFontSize(theme::kTypeH3);
        idx->setTextColor(theme::kDarkTextSecondary);
        idx->setMarginRight(12);
        row->addView(idx);

        auto* title = new brls::Label();
        title->setText(e.nameCN.empty() ? e.name : e.nameCN);
        title->setFontSize(theme::kTypeH3);
        title->setTextColor(theme::kDarkTextPrimary);
        title->setSingleLine(true);
        title->setGrow(1.0f);
        row->addView(title);

        if (e.duration > 0) {
            auto* dur = new brls::Label();
            dur->setText(format_duration(e.duration));
            dur->setFontSize(theme::kTypeCaption);
            dur->setTextColor(theme::kDarkTextSecondary);
            row->addView(dur);
        }

        int32_t eid = e.id;
        const std::string epTitle = e.nameCN.empty() ? e.name : e.nameCN;
        const int32_t sid = subjectId_;
        row->registerClickAction([eid, sid, epTitle](brls::View*) {
            Intent::openSourcePicker(eid, sid, epTitle);
            return true;
        });
        list_->addView(row);
    }
}

}  // namespace aniswitch
