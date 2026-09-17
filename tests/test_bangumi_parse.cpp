// SPDX-License-Identifier: AGPL-3.0
#include "net/bgm_types.hpp"
#include <cstdio>
#include <cstdlib>

#define EXPECT(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "FAIL: %s at %d\n", #condition, __LINE__); return 1; } } while (0)

int main() {
    using namespace aniswitch;
    using nlohmann::json;
    auto response = json::parse(R"({
        "id":1,"type":2,"name":"Cowboy Bebop","name_cn":"星际牛仔",
        "date":null,"eps":26,"total_episodes":27,
        "rating":{"score":9.0,"rank":50,"total":12345,"count":{"1":50,"10":2395}},
        "images":{"large":"L","common":"C"},
        "collection":{"wish":500,"collect":5000,"doing":1000,"on_hold":200,"dropped":100},
        "tags":[{"name":"SF","count":15}],"meta_tags":["TV","原创"]
    })");
    Subject subject = response.get<Subject>();
    EXPECT(subject.id == 1 && subject.nameCN == "星际牛仔");
    EXPECT(subject.totalEps == 27 && subject.date.empty());
    EXPECT(subject.images.common == "C" && subject.rating.count_10 == 2395);
    EXPECT(subject.collection.onHold == 200 && subject.tags.at(0) == "SF");
    EXPECT(subject.metaTags.at(0) == "TV");

    auto page = json{{"total", 1}, {"limit", 25}, {"offset", 0}, {"data", json::array({response})}};
    auto search = parseResponseData<SearchSubjectList>(page);
    EXPECT(search.total == 1 && search.data.size() == 1);
    EXPECT(search.data[0].nameCN == "星际牛仔" && search.data[0].score == 9.0);
    EXPECT(parseResponseData<std::vector<Subject>>(page).size() == 1);
    EXPECT(parseResponseData<std::vector<Subject>>(json{{"result", page["data"]}}).size() == 1);

    auto episodes = parseResponseData<std::vector<Episode>>(json::parse(R"({"total":1,"data":[{
        "id":8,"name":"SP","name_cn":"特别篇","sort":6.5,"ep":6.5,
        "duration":"00:24:00","duration_seconds":1440,"subject_id":1,"disc":0
    }]})"));
    EXPECT(episodes[0].sort == 6.5 && episodes[0].duration == 1440);
    EXPECT(episodes[0].subjectID == 1 && episodes[0].nameCN == "特别篇");
    auto collection = json::parse(R"({"subject_id":1,"rate":8,"type":3,"private":true,
        "comment":null,"ep_status":6,"updated_at":"2022-06-19T18:44:13+08:00",
        "subject":{"id":1,"name":"Test","name_cn":"测试","images":null}})").get<UserCollection>();
    EXPECT(collection.rating == 8 && collection.private_ && collection.epStatus == 6);
    EXPECT(collection.updatedAt == "2022-06-19T18:44:13+08:00" && collection.subject.nameCN == "测试");
    auto person = json::parse(R"({"id":3,"type":1,"career":["director"],"images":null})").get<Person>();
    EXPECT(person.type == 1 && person.career.at(0) == "director");
    auto character = json::parse(R"({"id":5,"relation":"主角","actors":[{"id":3,"name":"Actor"}]})").get<SubjectCharacter>();
    EXPECT(character.actorID == 3 && character.role == "主角");
    auto relation = json::parse(R"({"id":9,"name_cn":"续篇","relation":"续集","type":2})").get<SubjectRelation>();
    EXPECT(relation.subject.id == 9 && relation.type == "续集");

    bool rejected = false;
    try { json::parse(R"({"id":"invalid"})").get<Subject>(); }
    catch (const json::exception&) { rejected = true; }
    EXPECT(rejected);
    std::puts("test_bangumi_parse: OK (production deserializers)");
}
