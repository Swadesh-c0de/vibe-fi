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
                      " --print \"%(title)s|%(uploader)s|%(webpage_url)s|%(duration_string)s\"" +
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

            // Find pipes from the right: duration, URL, uploader, title
            size_t last_pipe = line.find_last_of('|');
            if (last_pipe == std::string::npos) continue;

            size_t second_last_pipe = line.find_last_of('|', last_pipe - 1);
            if (second_last_pipe == std::string::npos) continue;

            size_t third_last_pipe = line.find_last_of('|', second_last_pipe - 1);

            std::string raw_title;
            std::string uploader;
            if (third_last_pipe != std::string::npos) {
                raw_title = line.substr(0, third_last_pipe);
                uploader = line.substr(third_last_pipe + 1, second_last_pipe - third_last_pipe - 1);
            } else {
                raw_title = line.substr(0, second_last_pipe);
            }

            std::string title = sanitize_text(raw_title);
            std::string url = line.substr(second_last_pipe + 1, last_pipe - second_last_pipe - 1);
            std::string duration = line.substr(last_pipe + 1);

            if (!uploader.empty() && uploader != "NA") {
                if (uploader.length() > 8 && uploader.substr(uploader.length() - 8) == " - Topic") {
                    uploader = uploader.substr(0, uploader.length() - 8);
                }
                if (title.find(" - ") == std::string::npos && !uploader.empty()) {
                    title = uploader + " - " + title;
                }
            }

            SearchResult result;
            result.title = title;
            result.url = url;
            result.duration = (duration.empty() || duration == "NA") ? "--:--" : duration;

            if (!result.title.empty() && !result.url.empty()) {
                results.push_back(result);
            }
        }
    }

    return results;
}
