#pragma once

#include <string>
#include <vector>
#include <set>

std::vector<std::string> SplitIntoWords(const std::string& text);
std::vector<std::string_view> SplitIntoWordsView(const std::string_view text);

template <typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& string_views) {
    std::set<std::string, std::less<>> non_empty_strings;
    for(const std::string_view str : string_views) {
        if (!str.empty()) {
            non_empty_strings.insert(std::string{str});
        }
    }
    return non_empty_strings;
}