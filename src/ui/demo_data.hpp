// SPDX-License-Identifier: AGPL-3.0
//
// v22 compose-next: offline demo catalog. Real APIs first; on empty
// or error, screens render these rows so every page has visible
// content. Sentinel ids are negative so A-click will not hit the
// network with a fake Bangumi id.
#pragma once

#include <ctime>
#include <string>
#include <vector>
#include "net/bgm_types.hpp"
#include "utils/sqlite_store.hpp"

namespace aniswitch::demo {

inline const char* kBanner = "演示数据（网络/账号不可用）";

inline SearchSubject makeSubject(int32_t id, const std::string& nameCN,
                                 const std::string& name, double score) {
    SearchSubject s;
    s.id = id;
    s.nameCN = nameCN;
    s.name = name;
    s.score = score;
    return s;
}

inline std::vector<SearchSubject> subjects() {
    return {
        makeSubject(-1, "葬送的芙莉莲", "Sousou no Frieren", 9.1),
        makeSubject(-2, "孤独摇滚！", "Bocchi the Rock!", 8.8),
        makeSubject(-3, "间谍过家家", "SPY×FAMILY", 8.6),
        makeSubject(-4, "赛博朋克：边缘行者", "Cyberpunk: Edgerunners", 8.7),
        makeSubject(-5, "辉夜大小姐想让我告白", "Kaguya-sama", 8.9),
        makeSubject(-6, "灵能百分百", "Mob Psycho 100", 8.8),
        makeSubject(-7, "来自深渊", "Made in Abyss", 8.5),
        makeSubject(-8, "紫罗兰永恒花园", "Violet Evergarden", 8.7),
    };
}

inline Subject makeDetail(int32_t id, const std::string& nameCN) {
    Subject s;
    s.id = id;
    s.nameCN = nameCN;
    s.name = nameCN;
    s.summary =
        "这是离线演示条目。联网后打开任意首页海报即可看到真实简介、"
        "剧集与评分。演示数据仅用于保证各页在断网/未登录时仍有内容。";
    s.rating.score = 8.5f;
    s.rating.total = 10000;
    return s;
}

inline std::vector<Episode> episodesFor(int32_t subjectId) {
    std::vector<Episode> out;
    for (int i = 1; i <= 8; ++i) {
        Episode e;
        e.id = subjectId * 100 - i;  // stay negative
        e.sort = i;
        e.nameCN = "第 " + std::to_string(i) + " 话";
        e.duration = 1440;
        out.push_back(e);
    }
    return out;
}

inline std::vector<CalendarItem> calendar() {
    // Bangumi weekday 0=Mon .. 6=Sun; a few shows per day.
    static const char* kNames[] = {
        "周一演示番", "周二演示番", "周三演示番", "周四演示番",
        "周五演示番", "周六演示番", "周日演示番",
    };
    std::vector<CalendarItem> out;
    for (int d = 0; d < 7; ++d) {
        for (int k = 0; k < 2; ++k) {
            CalendarItem item;
            item.weekday = d;
            item.subject.id = -100 - d * 10 - k;
            item.subject.nameCN =
                std::string(kNames[d]) + (k == 0 ? " A" : " B");
            item.subject.name = item.subject.nameCN;
            item.subject.rating.score = 8.0f;
            out.push_back(item);
        }
    }
    return out;
}

inline std::vector<SQLiteStore::CollectionEntry> collection() {
    const auto base = subjects();
    std::vector<SQLiteStore::CollectionEntry> out;
    for (size_t i = 0; i < base.size(); ++i) {
        SQLiteStore::CollectionEntry e;
        e.subjectId = base[i].id;
        e.name = base[i].name;
        e.nameCN = base[i].nameCN;
        e.rating = static_cast<int32_t>(base[i].score);
        e.type = static_cast<int32_t>(1 + (i % 5));  // spread 5 states
        out.push_back(e);
    }
    return out;
}

inline std::vector<SQLiteStore::HistoryEntry> history() {
    const auto base = subjects();
    std::vector<SQLiteStore::HistoryEntry> out;
    for (size_t i = 0; i < 4 && i < base.size(); ++i) {
        SQLiteStore::HistoryEntry e;
        e.subjectId = base[i].id;
        e.episodeId = base[i].id * 100 - 1;
        e.subjectName = base[i].nameCN;
        e.episodeName = "第 1 话";
        e.positionMs = 300000 + static_cast<int64_t>(i) * 60000;
        e.durationMs = 1440000;
        e.watchedAt = static_cast<int64_t>(time(nullptr)) -
                      static_cast<int64_t>(i + 1) * 3600;
        out.push_back(e);
    }
    return out;
}

}  // namespace aniswitch::demo
