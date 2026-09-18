// SPDX-License-Identifier: AGPL-3.0
//
// v22 chrome: page root (kChromeBg + TV-safe margins + Switch width),
// name kTypeH1, alt/type captions on theme tokens, 出演作品 via
// chrome::makeSection, career chips theme::kTagCast, filmography as
// 6-col makePosterCard grid (DESIGN.md §5.4), HUD shell + B 返回.
// Presenter / HTTP logic unchanged.

#include "ui/activity/person_activity.hpp"
#include "ui/ui_chrome.hpp"
#include "ui/poster_card.hpp"
#include "ui/theme.hpp"
#include "utils/activity_helper.hpp"
#include "utils/image_loader.hpp"
#include "net/bgm_client.hpp"
#include <borealis/views/scrolling_frame.hpp>
#include <borealis/core/thread.hpp>
#include <fmt/format.h>

namespace aniswitch {

PersonActivity::PersonActivity(int32_t id) : personId_(id) {}
PersonActivity::~PersonActivity() = default;

void PersonActivity::onContentAvailable() {
    // Padded chrome page as scroll content (replaces ad-hoc root).
    auto* content = chrome::makePageRoot();

    // Header: avatar + name / alt-name / type captions.
    auto* top = new brls::Box();
    top->setAxis(brls::Axis::ROW);
    top->setMarginBottom(12);
    top->setAlignItems(brls::AlignItems::CENTER);

    avatar_ = new brls::Image();
    avatar_->setWidth(80);
    avatar_->setHeight(80);
    avatar_->setScalingType(brls::ImageScalingType::FILL);
    avatar_->setCornerRadius(40);
    avatar_->setMarginRight(16);
    top->addView(avatar_);

    auto* info = new brls::Box();
    info->setAxis(brls::Axis::COLUMN);
    info->setGrow(1);
    top->addView(info);

    // DESIGN.md §4: screen/detail primary name is type-h1.
    name_ = new brls::Label();
    name_->setFontSize(theme::kTypeH1);
    name_->setTextColor(theme::kDarkTextPrimary);
    name_->setSingleLine(false);
    name_->setText("加载中...");
    info->addView(name_);

    altName_ = new brls::Label();
    altName_->setFontSize(theme::kTypeCaption);
    altName_->setTextColor(theme::kDarkTextMuted);
    altName_->setSingleLine(true);
    info->addView(altName_);

    type_ = new brls::Label();
    type_->setFontSize(theme::kTypeCaption);
    type_->setTextColor(theme::kDarkTextSecondary);
    type_->setMarginTop(4);
    info->addView(type_);

    content->addView(top);

    // Career tags (e.g. "声优", "导演", "脚本") — non-interactive,
    // cast-chip surface token (kTagCast). Populated in onPerson.
    careerBox_ = new brls::Box();
    careerBox_->setAxis(brls::Axis::ROW);
    careerBox_->setAlignItems(brls::AlignItems::CENTER);
    careerBox_->setMarginBottom(8);
    content->addView(careerBox_);

    // Filmography section header.
    content->addView(chrome::makeSection("出演作品", 8));

    list_ = new brls::Box();
    list_->setAxis(brls::Axis::COLUMN);
    content->addView(list_);

    // HUD shell outside scroll + B 返回.
    auto* shell = chrome::attachScrollShell(this, content);
    if (shell) shell->setBackgroundColor(theme::kChromeBg);
    chrome::registerBack(this);

    presenter_.onPerson.subscribe([this](Person p) { onPerson(std::move(p)); });
    presenter_.onSubjects.subscribe([this](std::vector<SearchSubject> s) { onSubjects(std::move(s)); });
    presenter_.setPersonId(personId_);
    presenter_.refresh();

    chrome::finish(this, shell);
}

void PersonActivity::onPerson(Person p) {
    const std::string displayName = p.nameCN.empty() ? p.name : p.nameCN;
    name_->setText(displayName);
    altName_->setText(p.nameCN.empty() ? "" : p.name);
    const char* typeLabel = p.type == 1 ? "个人" : p.type == 2 ? "公司" : p.type == 3 ? "组合" : "";
    type_->setText(typeLabel);

    careerBox_->clearViews();
    for (const auto& c : p.career) {
        auto* tagBox = new brls::Box();
        tagBox->setAxis(brls::Axis::ROW);
        tagBox->setBackground(brls::ViewBackground::SHAPE_COLOR);
        tagBox->setBackgroundColor(theme::kTagCast);
        tagBox->setCornerRadius(10);
        tagBox->setMarginRight(6);
        tagBox->setMargins(2, 8, 2, 8);
        auto* tag = new brls::Label();
        tag->setText(c);
        tag->setFontSize(theme::kTypeCaption);
        tag->setTextColor(theme::kDarkTextPrimary);
        tagBox->addView(tag);
        careerBox_->addView(tagBox);
    }

    if (!p.image.empty()) {
        const std::string tag = fmt::format("person-avatar-{}", p.id);
        ImageLoader::instance().load(p.image,
            [this](const std::string& path) {
                if (path.empty()) return;
                brls::sync([this, path]() { avatar_->setImageFromFile(path); });
            }, tag);
    }
}

void PersonActivity::onSubjects(std::vector<SearchSubject> subjects) {
    list_->clearViews();
    if (subjects.empty()) {
        list_->addView(chrome::makeMuted("（暂无出演作品）", 8));
        return;
    }

    // v22 §5.4: 6-col poster grid — SearchSubject maps 1:1 onto
    // makePosterCardFromSearch (id / nameCN / score / images).
    constexpr int kCols = 6;
    for (size_t i = 0; i < subjects.size(); i += kCols) {
        auto* row = new brls::Box();
        row->setAxis(brls::Axis::ROW);
        row->setMarginBottom(24);
        for (size_t j = 0; j < kCols && i + j < subjects.size(); ++j) {
            const auto& s = subjects[i + j];
            const int32_t sid = s.id;
            row->addView(makePosterCardFromSearch(s, [sid]() {
                Intent::openSubject(sid);
            }));
        }
        list_->addView(row);
    }
}

}  // namespace aniswitch
