// SPDX-License-Identifier: AGPL-3.0

#include "ui/activity/episode_list_activity.hpp"
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
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setPadding(20);
    auto* title = new brls::Label();
    title->setText("剧集列表");
    title->setFontSize(28);
    title->setMarginBottom(10);
    root->addView(title);

    auto* list = new brls::Box();
    list->setAxis(brls::Axis::COLUMN);
    list->setId("eps_list");
    root->addView(list);
    list_ = list;
    auto* back = new brls::Button();
    back->setText("返回");
    back->registerClickAction([](brls::View*) { brls::Application::popActivity(); return true; });
    root->addView(back);
    auto* scroll = new brls::ScrollingFrame();
    scroll->setContentView(root);
    setContentView(scroll);
    registerAction("返回", brls::BUTTON_B, [](brls::View*) { brls::Application::popActivity(); return true; });

    presenter_.onEpisodes.subscribe([this](std::vector<Episode> e) { render(e); });
    presenter_.onLastWatched.subscribe([this, list](std::optional<SQLiteStore::HistoryEntry> last) {
        if (!last) return;
        // If we have a resume hint, jump straight in after the list
        // populates. The episode tap is the user-confirmable path.
        (void)list;
    });
    presenter_.setSubjectId(subjectId_);
    presenter_.refresh();
}

void EpisodeListActivity::render(const std::vector<Episode>& eps) {
    if (!list_) return;
    list_->clearViews();
    for (const auto& e : eps) {
        auto* row = new brls::Box();
        row->setFocusable(true);
        row->setAxis(brls::Axis::ROW);
        row->setMarginBottom(4);

        auto* idx = new brls::Label();
        idx->setText(fmt::format("EP{:g}  ", e.sort));
        idx->setFontSize(18);
        idx->setMarginRight(8);
        row->addView(idx);

        auto* title = new brls::Label();
        title->setText(e.nameCN.empty() ? e.name : e.nameCN);
        title->setFontSize(18);
        title->setGrow(1.0f);
        row->addView(title);

        if (e.duration > 0) {
            auto* dur = new brls::Label();
            dur->setText(format_duration(e.duration));
            dur->setFontSize(16);
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
