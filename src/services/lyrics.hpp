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

struct LyricCandidate {
    std::string track_name;
    std::string artist_name;
    std::string album_name;
    double duration = 0.0;
    std::string plain_lyrics;
    std::string synced_lyrics;
};

struct LyricCandidateResult {
    LyricsData data;
    LyricCandidate candidate;
    bool found = false;
};

class LyricsManager {
public:
    LyricsManager();
    LyricsData fetch_lyrics(const std::string& artist, const std::string& title, double duration = 0.0);
    bool get_cached_lyrics(const std::string& artist, const std::string& title, LyricsData& out);

private:
    std::string perform_request(const std::string& url);
    LyricsData parse_json_response(const std::string& json);
    double parse_timestamp(const std::string& timestamp_str);

    std::vector<LyricCandidate> parse_search_results(const std::string& json);
    LyricCandidateResult select_best_candidate(
        const std::vector<LyricCandidate>& candidates,
        const std::string& target_artist,
        const std::string& target_title,
        double target_duration);
};

#endif // LYRICS_HPP
