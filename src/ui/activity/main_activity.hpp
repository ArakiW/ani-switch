// SPDX-License-Identifier: AGPL-3.0
#pragma once
#include <borealis.hpp>
#include <functional>
#include <vector>
namespace aniswitch {
class MainActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/main_activity.xml");
    MainActivity();
    ~MainActivity() override;
    void onContentAvailable() override;

private:
    std::vector<brls::Button*> tabBtns_;
    std::vector<std::function<brls::View*()>> tabMakers_;
    brls::Box* content_ = nullptr;
    size_t activeTab_ = 0;
};
}
