// SPDX-License-Identifier: AGPL-3.0
#include "ui/activity/person_activity.hpp"
#include "ui/fragment/subject_cell.hpp"
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
    auto* scroll = new brls::ScrollingFrame();

    auto* top = new brls::Box();
    top->setAxis(brls::Axis::ROW);
    top->setPadding(20);

    avatar_ = new brls::Image();
    avatar_->setWidth(80);
    avatar_->setHeight(80);
    avatar_->setScalingType(brls::ImageScalingType::FILL);
    avatar_->setCornerRadius(40);
    top->addView(avatar_);

    auto* info = new brls::Box();
    info->setAxis(brls::Axis::COLUMN);
    info->setMarginLeft(16);
    info->setGrow(1);
    top->addView(info);

    name_ = new brls::Label();
    name_->setFontSize(20);
    name_->setSingleLine(false);
    name_->setText("加载中...");
    info->addView(name_);

    altName_ = new brls::Label();
    altName_->setFontSize(theme::kTypeCaption);
    altName_->setTextColor(nvgRGB(160, 160, 160));
    altName_->setSingleLine(true);
    info->addView(altName_);

    type_ = new brls::Label();
    type_->setFontSize(theme::kTypeCaption);
    type_->setTextColor(nvgRGB(180, 180, 180));
    type_->setMarginTop(4);
    info->addView(type_);

    scroll->addView(top);

    // Career tags (e.g. "声优", "导演", "脚本").
    careerBox_ = new brls::Box();
    careerBox_->setAxis(brls::Axis::ROW);
    careerBox_->setPadding(20, 0, 20, 0);
    scroll->addView(careerBox_);

    // Filmography header.
    auto* listHeader = new brls::Label();
    listHeader->setText("出演作品");
    listHeader->setFontSize(theme::kTypeH3);
    listHeader->setMarginLeft(20);
    listHeader->setMarginTop(16);
    listHeader->setMarginBottom(6);
    scroll->addView(listHeader);

    list_ = new brls::Box();
    list_->setAxis(brls::Axis::COLUMN);
    list_->setPadding(20, 0, 20, 20);
    scroll->addView(list_);

    setContentView(scroll);
    registerAction("返回", brls::BUTTON_B, [](brls::View*) { brls::Application::popActivity(); return true; });

    presenter_.onPerson.subscribe([this](Person p) { onPerson(std::move(p)); });
    presenter_.onSubjects.subscribe([this](std::vector<SearchSubject> s) { onSubjects(std::move(s)); });
    presenter_.setPersonId(personId_);
    presenter_.refresh();
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
        tagBox->setBackgroundColor(nvgRGBA(80, 60, 100, 200));
        tagBox->setCornerRadius(10);
        tagBox->setMarginRight(6);
        tagBox->setMargins(2, 8, 2, 8);
        auto* tag = new brls::Label();
        tag->setText(c);
        tag->setFontSize(theme::kTypeCaption);
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
        auto* empty = new brls::Label();
        empty->setText("（暂无出演作品）");
        empty->setFontSize(theme::kTypeCaption);
        empty->setTextColor(nvgRGB(160, 160, 160));
        list_->addView(empty);
        return;
    }
    for (const auto& s : subjects) {
        auto* cell = new SubjectCell();
        Subject subj;
        subj.id      = s.id;
        subj.name    = s.name;
        subj.nameCN  = s.nameCN;
        subj.summary = s.summary;
        if (s.score > 0) subj.rating.score = s.score;
        const std::string cover = !s.images.large.empty() ? s.images.large :
                                  !s.images.common.empty() ? s.images.common :
                                  !s.images.medium.empty() ? s.images.medium : std::string{};
        cell->setSubject(subj, cover);
        cell->setOnClick([](int32_t id) { Intent::openSubject(id); });
        list_->addView(cell);
    }
}

}  // namespace aniswitch
