#ifndef LYRICS_HPP
#define LYRICS_HPP

#include <string>
#include <vector>

struct LyricLine {
    double timestamp; // in seconds
    std::string text;
};

struct LyricsData {
    std::string plain_lyrics;
    std::vector<LyricLine> synced_lyrics;
    bool has_synced;
};

class LyricsManager {
public:
    LyricsManager();
    LyricsData fetch_lyrics(const std::string& artist, const std::string& title);

private:
    std::string perform_request(const std::string& url);
    LyricsData parse_json_response(const std::string& json);
    double parse_timestamp(const std::string& timestamp_str);
};

#endif // LYRICS_HPP
