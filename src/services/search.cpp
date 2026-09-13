#include "search.hpp"
#include "utils.hpp"
#include <array>
#include <sstream>
#include <iostream>

std::vector<SearchResult> search_youtube(const std::string& query, int limit) {
    std::vector<SearchResult> results;
    if (query.empty() || !is_online()) return results;

    std::string ytdl_path = find_executable("yt-dlp");
    std::string search_term = "ytsearch" + std::to_string(limit) + ":" + query;

    std::string cmd = shell_escape(ytdl_path) + 
                      " --print \"%(title)s|%(webpage_url)s|%(duration_string)s\"" +
                      " --flat-playlist --no-warnings " + 
                      shell_escape(search_term) + " 2>/dev/null";

    UniquePipe pipe(popen(cmd.c_str(), "r"));
    if (!pipe) {
        return results;
    }

    char buffer[2048];
    std::string accumulated;

    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        accumulated += buffer;
        size_t newline_pos;
        while ((newline_pos = accumulated.find('\n')) != std::string::npos) {
            std::string line = accumulated.substr(0, newline_pos);
            accumulated.erase(0, newline_pos + 1);

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty()) continue;

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

            if (result.duration.empty() || result.duration == "NA") {
                result.duration = "--:--";
            }

            if (!result.title.empty() && !result.url.empty()) {
                results.push_back(result);
            }
        }
    }

    return results;
}
