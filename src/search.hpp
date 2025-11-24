#ifndef SEARCH_HPP
#define SEARCH_HPP

#include <string>
#include <vector>

struct SearchResult {
    std::string title;
    std::string url;
    std::string duration;
};

std::vector<SearchResult> search_youtube(const std::string& query, int limit = 10);

#endif // SEARCH_HPP
