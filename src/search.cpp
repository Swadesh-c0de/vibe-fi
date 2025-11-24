#include "search.hpp"
#include "utils.hpp"
#include <array>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <sstream>

std::vector<SearchResult> search_youtube(const std::string& query, int limit) {
    std::vector<SearchResult> results;
    std::string cmd = "yt-dlp --print \"%(title)s|%(webpage_url)s|%(duration_string)s\" --flat-playlist \"ytsearch" + std::to_string(limit) + ":" + query + "\" 2>/dev/null";
    
    std::array<char, 1024> buffer;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        std::string line = buffer.data();
        if (!line.empty() && line.back() == '\n') {
            line.pop_back();
        }
        
        // Find the last two pipe characters to extract URL and duration
        // This handles titles that contain pipe characters
        size_t last_pipe = line.find_last_of('|');
        if (last_pipe == std::string::npos) continue;
        
        size_t second_last_pipe = line.find_last_of('|', last_pipe - 1);
        if (second_last_pipe == std::string::npos) continue;
        
        SearchResult result;
        result.title = sanitize_text(line.substr(0, second_last_pipe));
        result.url = line.substr(second_last_pipe + 1, last_pipe - second_last_pipe - 1);
        result.duration = line.substr(last_pipe + 1);
        results.push_back(result);
    }
    
    return results;
}
