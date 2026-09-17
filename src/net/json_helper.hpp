// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
//
// JSON helpers used across the network layer. The NLOHMANN_JSON_FROM
// redefinition below makes from_json tolerant to missing/null fields so
// upstream API changes don't crash us.

#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <type_traits>
#include <vector>

// Re-define from_json to skip null / missing keys instead of throwing.
#undef NLOHMANN_JSON_FROM
#define NLOHMANN_JSON_FROM(v1) { auto iter = nlohmann_json_j.find(#v1); \
    if (iter != nlohmann_json_j.end() && !iter->is_null()) iter->get_to(nlohmann_json_t.v1); }

namespace aniswitch {

template <typename T>
void readJsonField(const nlohmann::json& json, const char* key, T& value) {
    auto it = json.find(key);
    if (it != json.end() && !it->is_null()) it->get_to(value);
}

template <typename T> struct IsJsonVector : std::false_type {};
template <typename T, typename A> struct IsJsonVector<std::vector<T, A>> : std::true_type {};

template <typename T>
T parseResponseData(const nlohmann::json& json) {
    if (json.is_object() && json.contains("result"))
        return json.at("result").get<T>();
    if constexpr (IsJsonVector<T>::value) {
        if (json.is_object() && json.contains("data"))
            return json.at("data").get<T>();
    }
    return json.get<T>();
}

// Resolve a possibly-relative URL against a base.
std::string parseLink(const std::string& url);

}  // namespace aniswitch
