// SPDX-License-Identifier: AGPL-3.0
#include "ui/activity/subject_activity.hpp"
#include "ui/theme.hpp"
#include "ui/hud.hpp"
#include "ui/demo_data.hpp"
#include "utils/activity_helper.hpp"
#include "utils/image_loader.hpp"
#include "net/bgm_client.hpp"
#include <borealis/views/scrolling_frame.hpp>
#include <borealis/core/thread.hpp>
#include <algorithm>
#include <fmt/format.h>

namespace aniswitch {

SubjectActivity::SubjectActivity(int32_t id, const std::string&) : subjectId_(id) {}
SubjectActivity::~SubjectActivity() = default;

void SubjectActivity::onContentAvailable() {
    auto* scroll = new brls::ScrollingFrame();

    // Top section: cover + title + meta.
    auto* top = new brls::Box();
    top->setAxis(brls::Axis::ROW);
    top->setPadding(20);

    cover_ = new brls::Image();
    // v22 §5.6: detail cover 200×300 r8.
    cover_->setWidth(200);
    cover_->setHeight(300);
    cover_->setScalingType(brls::ImageScalingType::FILL);
    cover_->setCornerRadius(8);
    top->addView(cover_);

    auto* info = new brls::Box();
    info->setAxis(brls::Axis::COLUMN);
    info->setMarginLeft(16);
    info->setGrow(1);
    top->addView(info);

    title_ = new brls::Label();
    title_->setFontSize(theme::kTypeH1);
    title_->setSingleLine(false);
    title_->setText("加载中...");
    info->addView(title_);

    altTitle_ = new brls::Label();
    altTitle_->setFontSize(theme::kTypeCaption);
    altTitle_->setTextColor(theme::kDarkTextSecondary);
    altTitle_->setSingleLine(true);
    info->addView(altTitle_);

    rating_ = new brls::Label();
    rating_->setFontSize(theme::kTypeH3);
    rating_->setTextColor(nvgRGB(255, 200, 80));
    rating_->setMarginTop(6);
    info->addView(rating_);

    meta_ = new brls::Label();
    meta_->setFontSize(theme::kTypeCaption);
    meta_->setTextColor(theme::kDarkTextSecondary);
    meta_->setSingleLine(false);
    info->addView(meta_);

    scroll->addView(top);

    // Tags row (horizontal chips, populated after subject loads).
    tagsBox_ = new brls::Box();
    tagsBox_->setAxis(brls::Axis::ROW);
    tagsBox_->setPadding(20, 0, 20, 0);
    scroll->addView(tagsBox_);

    // Summary.
    auto* summaryLabel = new brls::Label();
    summaryLabel->setText("简介");
    summaryLabel->setFontSize(theme::kTypeH2);
    summaryLabel->setMarginTop(16);
    summaryLabel->setMarginLeft(20);
    summaryLabel->setMarginBottom(6);
    scroll->addView(summaryLabel);

    summary_ = new brls::Label();
    summary_->setFontSize(theme::kTypeBody);
    summary_->setSingleLine(false);
    summary_->setMarginLeft(20);
    summary_->setMarginRight(20);
    summary_->setMarginBottom(12);
    scroll->addView(summary_);

    // Episode list header.
    auto* epLabel = new brls::Label();
    epLabel->setText("剧集");
    epLabel->setFontSize(theme::kTypeH2);
    epLabel->setMarginLeft(20);
    epLabel->setMarginBottom(6);
    scroll->addView(epLabel);

    episodeList_ = new brls::Box();
    episodeList_->setAxis(brls::Axis::COLUMN);
    episodeList_->setPadding(20, 0, 20, 20);
    scroll->addView(episodeList_);

    // v22 §6.4: HUD shell outside the scroll frame.
    auto* shell = setContentViewWithHudShell(this, scroll);
    registerAction("返回", brls::BUTTON_B, [](brls::View*) { brls::Application::popActivity(); return true; });
    registerAction("打开", brls::BUTTON_A, [](brls::View*) { return true; });
    registerAction("收藏", brls::BUTTON_Y, [](brls::View*) { return true; });
    appendHud(this, shell);

    // Wire presenter callbacks.
    presenter_.onSubject.subscribe([this](Subject s) { onSubject(std::move(s)); });
    presenter_.onEpisodes.subscribe([this](std::vector<Episode> e) { onEpisodes(std::move(e)); });
    presenter_.onCharacters.subscribe([this](std::vector<SubjectCharacter> c) { onCharacters(std::move(c)); });
    presenter_.onRelations.subscribe([this](std::vector<SubjectRelation> r) { onRelations(std::move(r)); });
    presenter_.onComments.subscribe([this](std::vector<Comment> v) { onComments(std::move(v)); });
    presenter_.onRating.subscribe([this](SubjectRating r) { onRating(std::move(r)); });
    presenter_.onMyCollection.subscribe([this](UserCollection c) { onMyCollection(std::move(c)); });
    presenter_.setSubjectId(subjectId_);
    if (subjectId_ < 0) {
        // v22 compose-next: demo sentinel ids skip the network.
        std::string name = "演示条目";
        for (const auto& s : demo::subjects()) {
            if (s.id == subjectId_) {
                name = s.nameCN.empty() ? s.name : s.nameCN;
                break;
            }
        }
        onSubject(demo::makeDetail(subjectId_, name));
        onEpisodes(demo::episodesFor(subjectId_));
        meta_->setText(demo::kBanner);
    } else {
        presenter_.refresh();
    }
}

void SubjectActivity::onSubject(Subject s) {
    subject_ = s;
    const std::string displayName = s.nameCN.empty() ? s.name : s.nameCN;
    title_->setText(displayName);
    altTitle_->setText(s.nameCN.empty() ? "" : s.name);
    if (s.rating.score > 0) {
        rating_->setText(fmt::format("★ {:.1f}  ({} 人)", s.rating.score, s.rating.total));
    } else {
        rating_->setText("");
    }
    std::string metaText;
    if (s.eps > 0 || s.totalEps > 0) {
        const int total = s.totalEps > 0 ? s.totalEps : s.eps;
        metaText += fmt::format("共 {} 集", total);
    }
    if (!s.date.empty()) {
        if (!metaText.empty()) metaText += "  ·  ";
        metaText += s.date;
    }
    if (!s.platform.empty()) {
        if (!metaText.empty()) metaText += "  ·  ";
        metaText += s.platform;
    }
    meta_->setText(metaText);
    summary_->setText(s.summary.empty() ? "（无简介）" : s.summary);

    // Re-render tags and load cover.
    tagsBox_->clearViews();
    for (const auto& t : s.tags) {
        auto* tagBox = new brls::Box();
        tagBox->setAxis(brls::Axis::ROW);
        tagBox->setBackground(brls::ViewBackground::SHAPE_COLOR);
        tagBox->setBackgroundColor(nvgRGBA(60, 80, 120, 200));
        tagBox->setCornerRadius(10);
        tagBox->setMarginRight(6);
        tagBox->setMargins(2, 8, 2, 8);
        auto* tag = new brls::Label();
        tag->setText(t);
        tag->setFontSize(theme::kTypeCaption);
        tagBox->addView(tag);
        tagsBox_->addView(tagBox);
    }

    // Cover image — gated by the same kCoverEnabled switch as SubjectCell.
    if (theme::kCoverEnabled &&
        (!s.images.large.empty() || !s.images.common.empty() || !s.images.medium.empty())) {
        const std::string url = !s.images.large.empty() ? s.images.large
                                : !s.images.common.empty() ? s.images.common
                                : s.images.medium;
        const std::string tag = fmt::format("subject-cover-{}", s.id);
        ImageLoader::instance().load(url,
            [this](const std::string& path) {
                if (path.empty()) return;
                brls::sync([this, path]() { cover_->setImageFromFile(path); });
            }, tag);
    }
}

void SubjectActivity::onEpisodes(std::vector<Episode> episodes) {
    episodeList_->clearViews();
    if (episodes.empty()) {
        auto* empty = new brls::Label();
        empty->setText("（暂无剧集信息）");
        empty->setFontSize(theme::kTypeBody);
        empty->setTextColor(theme::kDarkTextSecondary);
        episodeList_->addView(empty);
        return;
    }
    // Sort by sort number just in case.
    std::sort(episodes.begin(), episodes.end(),
              [](const Episode& a, const Episode& b) { return a.sort < b.sort; });
    for (size_t i = 0; i < episodes.size() && i < 64; ++i) {
        const Episode& e = episodes[i];
        const int32_t epId = e.id;
        auto* row = new brls::Box();
        row->setAxis(brls::Axis::ROW);
        row->setPadding(12, 10, 12, 10);
        row->setBackground(brls::ViewBackground::SHAPE_COLOR);
        row->setBackgroundColor(theme::kDarkCardRowBg);
        row->setCornerRadius(8);
        row->setMarginBottom(6);
        row->setHeight(theme::kRowHeight);
        row->setFocusable(true);
        theme::applyFocusStyle(row, 8.0f);
        row->registerClickAction([epId](brls::View*) {
            Intent::openPlayer(epId);
            return true;
        });
        auto* idx = new brls::Label();
        idx->setText(e.ep > 0 ? fmt::format("第 {:.0f} 话", e.ep) :
                              (e.sort > 0 ? fmt::format("#{:.0f}", e.sort) : "SP"));
        idx->setFontSize(theme::kTypeH3);
        idx->setTextColor(nvgRGB(255, 200, 80));
        idx->setWidth(96);
        row->addView(idx);
        auto* name = new brls::Label();
        name->setText(e.nameCN.empty() ? e.name : e.nameCN);
        name->setFontSize(theme::kTypeH3);
        name->setSingleLine(true);
        name->setGrow(1);
        row->addView(name);
        if (e.duration > 0) {
            auto* dur = new brls::Label();
            dur->setText(fmt::format("{} 分", e.duration / 60));
            dur->setFontSize(theme::kTypeCaption);
            dur->setTextColor(theme::kDarkTextSecondary);
            row->addView(dur);
        }
        episodeList_->addView(row);
    }
}

void SubjectActivity::onCharacters(std::vector<SubjectCharacter> characters) {
    if (characters.empty()) return;
    // v17.1: full cast list, grouped by role.  Bangumi returns
    // a flat SubjectCharacter list; we group by `type` (1=主角,
    // 2=配角, 3=客串) and by character.  The previous v16.0.8.1
    // build only rendered the first 8 actors as a chip row —
    // not enough on subjects with 20+ cast members.
    auto* scroll = dynamic_cast<brls::ScrollingFrame*>(getContentView());
    if (!scroll) return;

    auto* castHeader = new brls::Label();
    castHeader->setText(fmt::format("声优 / 出演 ({})", characters.size()));
    castHeader->setFontSize(theme::kTypeH2);
    castHeader->setMarginLeft(20);
    castHeader->setMarginTop(10);
    castHeader->setMarginBottom(6);
    scroll->addView(castHeader);

    auto* list = new brls::Box();
    list->setAxis(brls::Axis::COLUMN);
    list->setPadding(20, 0, 20, 12);
    for (const auto& c : characters) {
        auto* row = new brls::Box();
        row->setAxis(brls::Axis::ROW);
        row->setPadding(12, 10, 12, 10);
        row->setMarginBottom(6);
        row->setBackground(brls::ViewBackground::SHAPE_COLOR);
        row->setBackgroundColor(aniswitch::theme::kDarkCardRowBg);
        row->setCornerRadius(8);
        row->setHeight(theme::kRowHeight);
        row->setFocusable(true);
        theme::applyFocusStyle(row, 8.0f);
        // Build a stable text label out of role + actor name.
        // role is the Chinese label for `type`; if `relation`
        // is non-empty (e.g. "主角") we show that, otherwise
        // we show the actor name + role suffix.
        std::string roleText = c.role.empty() ? "出演" : c.role;
        auto* role = new brls::Label();
        role->setText(roleText);
        role->setFontSize(theme::kTypeCaption);
        role->setTextColor(aniswitch::theme::kAccent);
        role->setMarginRight(8);
        row->addView(role);

        auto* name = new brls::Label();
        // Prefer Chinese name when present, fall back to original.
        name->setText(c.nameCN.empty() ? c.name : c.nameCN);
        name->setFontSize(theme::kTypeH3);
        name->setTextColor(aniswitch::theme::kDarkTextPrimary);
        // The character itself is clickable (SubjectCharacter.id is
        // the character id, not a person/actor id — for now we just
        // open the person page if the parser populated one, but the
        // upstream Bangumi /v0/characters/{id} endpoint isn't wired
        // yet so this stays as a static label until v17.x).
        row->addView(name);
        list->addView(row);
    }
    scroll->addView(list);
}

void SubjectActivity::onRelations(std::vector<SubjectRelation> relations) {
    if (relations.empty()) return;
    // v17.1: relations section.  Bangumi's /v0/subjects/{id}/subjects
    // endpoint returns "this X is related to Y" pairs.  Each
    // SubjectRelation nests a full Subject so we read name + id from
    // `r.subject`.
    auto* scroll = dynamic_cast<brls::ScrollingFrame*>(getContentView());
    if (!scroll) return;

    auto* header = new brls::Label();
    header->setText(fmt::format("关联作品 ({})", relations.size()));
    header->setFontSize(theme::kTypeH2);
    header->setMarginLeft(20);
    header->setMarginTop(6);
    header->setMarginBottom(6);
    scroll->addView(header);

    auto* list = new brls::Box();
    list->setAxis(brls::Axis::COLUMN);
    list->setPadding(20, 0, 20, 12);
    for (const auto& r : relations) {
        auto* row = new brls::Box();
        row->setAxis(brls::Axis::ROW);
        row->setPadding(12, 10, 12, 10);
        row->setMarginBottom(6);
        row->setBackground(brls::ViewBackground::SHAPE_COLOR);
        row->setBackgroundColor(aniswitch::theme::kDarkCardRowBg);
        row->setCornerRadius(8);
        row->setHeight(theme::kRowHeight);
        row->setFocusable(true);
        theme::applyFocusStyle(row, 8.0f);

        auto* rel = new brls::Label();
        rel->setText(r.type.empty() ? "关联" : r.type);
        rel->setFontSize(theme::kTypeCaption);
        rel->setTextColor(aniswitch::theme::kAccent);
        rel->setMarginRight(8);
        row->addView(rel);

        auto* name = new brls::Label();
        name->setText(r.subject.name);
        name->setFontSize(theme::kTypeH3);
        name->setTextColor(aniswitch::theme::kDarkTextPrimary);
        const int32_t destId = r.subject.id;
        if (destId > 0) {
            row->registerClickAction([destId](brls::View*) {
                Intent::openSubject(destId);
                return true;
            });
        }
        row->addView(name);
        list->addView(row);
    }
    scroll->addView(list);
}

void SubjectActivity::onComments(std::vector<Comment> comments) {
    auto* scroll = dynamic_cast<brls::ScrollingFrame*>(getContentView());
    if (!scroll) return;

    auto* header = new brls::Label();
    header->setText(fmt::format("短评 ({})", comments.size()));
    header->setFontSize(theme::kTypeH2);
    header->setMarginLeft(20);
    header->setMarginTop(12);
    header->setMarginBottom(6);
    scroll->addView(header);

    auto* list = new brls::Box();
    list->setAxis(brls::Axis::COLUMN);
    list->setPadding(20, 0, 20, 20);
    if (comments.empty()) {
        auto* empty = new brls::Label();
        empty->setText("（暂无短评）");
        empty->setFontSize(theme::kTypeBody);
        empty->setTextColor(theme::kDarkTextSecondary);
        list->addView(empty);
    } else {
        for (const auto& c : comments) {
            auto* card = new brls::Box();
            card->setAxis(brls::Axis::COLUMN);
            card->setPadding(12);
            card->setBackground(brls::ViewBackground::SHAPE_COLOR);
            card->setBackgroundColor(theme::kDarkCardRowBg);
            card->setCornerRadius(8);
            card->setMarginBottom(8);
            auto* top = new brls::Box();
            top->setAxis(brls::Axis::ROW);
            top->setMarginBottom(4);
            auto* name = new brls::Label();
            name->setText(c.user.nickname.empty() ? c.user.username : c.user.nickname);
            name->setFontSize(theme::kTypeH3);
            name->setTextColor(nvgRGB(255, 200, 80));
            top->addView(name);
            if (c.rate > 0) {
                auto* rate = new brls::Label();
                rate->setText(fmt::format("  ★ {}", c.rate));
                rate->setFontSize(theme::kTypeCaption);
                rate->setTextColor(nvgRGB(255, 200, 80));
                top->addView(rate);
            }
            card->addView(top);
            auto* body = new brls::Label();
            body->setText(c.content);
            body->setFontSize(theme::kTypeBody);
            body->setSingleLine(false);
            card->addView(body);
            if (!c.createdAt.empty()) {
                auto* time = new brls::Label();
                time->setText(c.createdAt.substr(0, 10));
                time->setFontSize(theme::kTypeCaption);
                time->setTextColor(theme::kDarkTextMuted);
                time->setMarginTop(4);
                card->addView(time);
            }
            list->addView(card);
        }
    }
    scroll->addView(list);
}

// v17.2: render aggregate rating + my-collection status.  Both
// are compact rows at the top of the page (right after the
// summary) so the user sees the score and "in your collection"
// state without scrolling through the cast/relations list.
void SubjectActivity::onRating(SubjectRating rating) {
    if (rating.total <= 0 || rating.scoreText.empty()) return;
    auto* scroll = dynamic_cast<brls::ScrollingFrame*>(getContentView());
    if (!scroll) return;

    auto* row = new brls::Box();
    row->setAxis(brls::Axis::ROW);
    row->setPadding(20, 10, 20, 4);
    row->setMarginBottom(4);
    auto* score = new brls::Label();
    score->setText(fmt::format("评分: {}  ({} 人)", rating.scoreText, rating.total));
    score->setFontSize(theme::kTypeH3);
    score->setTextColor(aniswitch::theme::kDarkCardHighlight);
    row->addView(score);
    scroll->addView(row);
}

void SubjectActivity::onMyCollection(UserCollection c) {
    if (c.subjectId == 0) return;  // 404 = not in collection
    auto* scroll = dynamic_cast<brls::ScrollingFrame*>(getContentView());
    if (!scroll) return;

    static const char* kTypeNames[] = {
        "", "想看", "在看", "看过", "搁置", "抛弃",
    };
    const char* typeName = (c.type >= 1 && c.type <= 5)
        ? kTypeNames[c.type] : "未知";

    auto* row = new brls::Box();
    row->setAxis(brls::Axis::ROW);
    row->setPadding(20, 2, 20, 4);
    row->setMarginBottom(8);
    auto* tag = new brls::Label();
    tag->setText(fmt::format("我的收藏: {}  ", typeName));
    tag->setFontSize(theme::kTypeCaption);
    tag->setTextColor(aniswitch::theme::kAccent);
    row->addView(tag);
    if (c.rating > 0) {
        auto* myScore = new brls::Label();
        myScore->setText(fmt::format("我的评分: {}/10  ", c.rating));
        myScore->setFontSize(theme::kTypeCaption);
        myScore->setTextColor(aniswitch::theme::kDarkTextSecondary);
        row->addView(myScore);
    }
    if (c.epStatus > 0) {
        auto* ep = new brls::Label();
        ep->setText(fmt::format("看到第 {} 话  ", c.epStatus));
        ep->setFontSize(theme::kTypeCaption);
        ep->setTextColor(aniswitch::theme::kDarkTextSecondary);
        row->addView(ep);
    }
    scroll->addView(row);

    // v18.9: 5-state chip row.  Animeko's CollectionEdit dialog
    // is a single sheet; we render it inline so the user can
    // see the current state and tap a new state without an
    // extra layer.  Each button calls
    // editor_.setCollectionType(subjectId, type) which hits
    // PATCH /v0/users/-/collections/{id} and re-fires the
    // onMyCollection chain on success.
    static const char* kShortNames[] = {"", "想看", "在看", "看过", "搁置", "抛弃"};
    auto* chipRow = new brls::Box();
    chipRow->setAxis(brls::Axis::ROW);
    chipRow->setPadding(20, 2, 20, 4);
    chipRow->setMarginBottom(8);
    int32_t subjId = c.subjectId;
    for (int t = 1; t <= 5; ++t) {
        auto* chip = new brls::Button();
        const char* arrow = (t == c.type) ? "▶ " : "  ";
        chip->setText(fmt::format("{}{}", arrow, kShortNames[t]));
        chip->setHeight(theme::kButtonHeight);
        theme::applyFocusStyle(chip, 8.0f);
        if (t == c.type) {
            // Highlight the active state with the highlight
            // style (filled accent, white text).
            chip->setStyle(&brls::BUTTONSTYLE_HIGHLIGHT);
        }
        chip->registerClickAction([this, subjId, t](brls::View*) {
            editor_.setCollectionType(subjId, t);
            return true;
        });
        chipRow->addView(chip);
    }
    scroll->addView(chipRow);
}

}  // namespace aniswitch
