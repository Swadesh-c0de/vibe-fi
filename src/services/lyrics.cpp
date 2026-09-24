#include "lyrics.hpp"
#include "utils.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <array>
#include <cmath>

namespace fs = std::filesystem;

static void save_lyrics_to_cache(const std::string& cache_file, const LyricCandidate& cand) {
    auto escape_json = [](const std::string& s) -> std::string {
        std::string res;
        res.reserve(s.size() * 2);
        for (char c : s) {
            if (c == '"') res += "\\\"";
            else if (c == '\\') res += "\\\\";
            else if (c == '\n') res += "\\n";
            else if (c == '\r') res += "\\r";
            else if (c == '\t') res += "\\t";
            else res += c;
        }
        return res;
    };

    std::ofstream out(cache_file);
    if (out.is_open()) {
        out << "{\n";
        out << "  \"trackName\": \"" << escape_json(cand.track_name) << "\",\n";
        out << "  \"artistName\": \"" << escape_json(cand.artist_name) << "\",\n";
        out << "  \"plainLyrics\": \"" << escape_json(cand.plain_lyrics) << "\",\n";
        out << "  \"syncedLyrics\": \"" << escape_json(cand.synced_lyrics) << "\"\n";
        out << "}\n";
    }
}

static std::string url_encode(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.length() * 2);
    for (unsigned char c : value) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped += c;
        } else if (c == ' ') {
            escaped += "+";
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", c);
            escaped += buf;
        }
    }
    return escaped;
}

LyricsManager::LyricsManager() {}

bool LyricsManager::get_cached_lyrics(const std::string& artist, const std::string& title, LyricsData& out) {
    if (title.empty()) return false;

    std::string clean_artist, clean_title;
    parse_artist_and_title(title, artist, clean_artist, clean_title);
    if (clean_title.empty()) clean_title = title;

    std::string cache_dir = get_vibe_dir() + "/cache/lyrics/";
    std::vector<std::string> candidate_artists;
    if (!clean_artist.empty() && clean_artist != "Unknown") {
        candidate_artists.push_back(clean_artist);
        size_t delim = clean_artist.find_first_of(",&");
        if (delim != std::string::npos) {
            std::string primary = clean_artist.substr(0, delim);
            while (!primary.empty() && primary.back() == ' ') primary.pop_back();
            if (!primary.empty() && primary != clean_artist) {
                candidate_artists.push_back(primary);
            }
        }
    }

    std::error_code ec;
    std::string safe_title = url_encode(clean_title);

    // 1. Direct candidate artist file checks
    for (const auto& a : candidate_artists) {
        std::string safe_artist = url_encode(a);
        std::string cache_file = cache_dir + safe_artist + "_" + safe_title + ".json";
        if (fs::exists(cache_file, ec)) {
            std::ifstream in(cache_file);
            if (in.is_open()) {
                std::stringstream buffer;
                buffer << in.rdbuf();
                std::string cached_json = buffer.str();
                if (!cached_json.empty() && cached_json.front() == '[') {
                    in.close();
                    fs::remove(cache_file, ec);
                    continue;
                }
                if (!cached_json.empty() && cached_json.front() == '{' && cached_json.find("\"error\"") == std::string::npos) {
                    LyricsData parsed = parse_json_response(cached_json);
                    if (parsed.has_synced || (!parsed.plain_lyrics.empty() && parsed.plain_lyrics != "Lyrics not found for this track.")) {
                        out = std::move(parsed);
                        return true;
                    }
                }
            }
        }
    }

    // 2. Fallback: match by title suffix (_<safe_title>.json) in cache directory
    if (!safe_title.empty() && fs::exists(cache_dir, ec)) {
        std::string suffix = "_" + safe_title + ".json";
        for (const auto& entry : fs::directory_iterator(cache_dir, ec)) {
            if (!entry.is_regular_file()) continue;
            std::string fname = entry.path().filename().string();
            if (fname.length() >= suffix.length() && 
                fname.compare(fname.length() - suffix.length(), suffix.length(), suffix) == 0) {
                std::ifstream in(entry.path());
                if (in.is_open()) {
                    std::stringstream buffer;
                    buffer << in.rdbuf();
                    std::string cached_json = buffer.str();
                    if (!cached_json.empty() && cached_json.front() == '{' && cached_json.find("\"error\"") == std::string::npos) {
                        LyricsData parsed = parse_json_response(cached_json);
                        if (parsed.has_synced || (!parsed.plain_lyrics.empty() && parsed.plain_lyrics != "Lyrics not found for this track.")) {
                            out = std::move(parsed);
                            return true;
                        }
                    }
                }
            }
        }
    }

    return false;
}

LyricsData LyricsManager::fetch_lyrics(const std::string& artist, const std::string& title, double duration) {
    if (title.empty()) {
        return {"Song title missing.", {}, false};
    }

    // Fast check: return from disk cache in < 0.1ms without any network calls
    LyricsData cached;
    if (get_cached_lyrics(artist, title, cached)) {
        return cached;
    }

    std::string clean_artist, clean_title;
    parse_artist_and_title(title, artist, clean_artist, clean_title);
    if (clean_title.empty()) clean_title = title;

    std::string safe_artist = url_encode(clean_artist.empty() ? "Unknown" : clean_artist);
    std::string safe_title = url_encode(clean_title);

    std::string cache_dir = get_vibe_dir() + "/cache/lyrics/";
    std::error_code ec;
    fs::create_directories(cache_dir, ec);
    std::string cache_file = cache_dir + safe_artist + "_" + safe_title + ".json";

    if (safe_artist == "Unknown" && fs::exists(cache_file, ec)) {
        // Automatically purge any ambiguous Unknown_<title>.json cache file
        fs::remove(cache_file, ec);
    }

    // If not cached and offline, do not stall on network requests
    if (!is_online()) {
        return {"Internet connection issue: Cannot fetch lyrics offline.", {}, false};
    }

    std::string response;
    std::string dur_param = (duration > 10.0) ? ("&duration=" + std::to_string(static_cast<int>(std::round(duration)))) : "";

    // 1. Direct query with artist & title + duration if artist is available
    if (!clean_artist.empty() && clean_artist != "Unknown") {
        std::string url = "https://lrclib.net/api/get?artist_name=" + safe_artist + "&track_name=" + safe_title + dur_param;
        response = perform_request(url);
    }

    // 2. Direct query with primary artist if artist has multiple artists (e.g. "Alan Walker, K-391...")
    if (response.empty() || response.find("\"error\"") != std::string::npos || response.find("404") != std::string::npos) {
        size_t delim = clean_artist.find_first_of(",&");
        if (delim != std::string::npos) {
            std::string primary_artist = clean_artist.substr(0, delim);
            while (!primary_artist.empty() && primary_artist.back() == ' ') primary_artist.pop_back();
            if (!primary_artist.empty()) {
                std::string url = "https://lrclib.net/api/get?artist_name=" + url_encode(primary_artist) + "&track_name=" + safe_title + dur_param;
                response = perform_request(url);
            }
        }
    }

    // 3. Direct query without duration parameter if duration-constrained get failed
    if ((response.empty() || response.find("\"error\"") != std::string::npos || response.find("404") != std::string::npos) && !dur_param.empty()) {
        if (!clean_artist.empty() && clean_artist != "Unknown") {
            std::string url = "https://lrclib.net/api/get?artist_name=" + safe_artist + "&track_name=" + safe_title;
            response = perform_request(url);
        }
    }

    // Check if direct get succeeded
    if (!response.empty() && response.front() == '{' && response.find("\"error\"") == std::string::npos && response.find("404") == std::string::npos) {
        LyricsData parsed = parse_json_response(response);
        if (parsed.has_synced || (!parsed.plain_lyrics.empty() && parsed.plain_lyrics != "Lyrics not found for this track.")) {
            std::ofstream out(cache_file);
            if (out.is_open()) out << response;
            return parsed;
        }
    }

    // Helper to save candidate to clean cache file with its verified artist
    auto persist_best_candidate = [&](const LyricCandidate& cand) {
        std::string target_artist = clean_artist.empty() ? cand.artist_name : clean_artist;
        if (!target_artist.empty() && target_artist != "Unknown") {
            std::string target_cache_file = cache_dir + url_encode(target_artist) + "_" + safe_title + ".json";
            save_lyrics_to_cache(target_cache_file, cand);
        }
    };

    // 4. Targeted search: api/search?track_name=...&artist_name=...
    if (!clean_artist.empty() && clean_artist != "Unknown") {
        std::string url = "https://lrclib.net/api/search?track_name=" + safe_title + "&artist_name=" + safe_artist;
        response = perform_request(url);
        if (!response.empty() && response.front() == '[') {
            auto candidates = parse_search_results(response);
            auto best = select_best_candidate(candidates, clean_artist, clean_title, duration);
            if (best.found && (best.data.has_synced || (!best.data.plain_lyrics.empty() && best.data.plain_lyrics != "Lyrics not found for this track."))) {
                persist_best_candidate(best.candidate);
                return best.data;
            }
        }
    }

    // 5. Query search: api/search?q=...
    std::string query_str = (!clean_artist.empty() && clean_artist != "Unknown") ? (clean_artist + " " + clean_title) : clean_title;
    std::string search_url = "https://lrclib.net/api/search?q=" + url_encode(query_str);
    response = perform_request(search_url);
    if (!response.empty() && response.front() == '[') {
        auto candidates = parse_search_results(response);
        auto best = select_best_candidate(candidates, clean_artist, clean_title, duration);
        if (best.found && (best.data.has_synced || (!best.data.plain_lyrics.empty() && best.data.plain_lyrics != "Lyrics not found for this track."))) {
            persist_best_candidate(best.candidate);
            return best.data;
        }
    }

    // 6. Title-only search with duration scoring
    std::string fallback_url = "https://lrclib.net/api/search?track_name=" + safe_title;
    response = perform_request(fallback_url);
    if (!response.empty() && response.front() == '[') {
        auto candidates = parse_search_results(response);
        auto best = select_best_candidate(candidates, clean_artist, clean_title, duration);
        if (best.found && (best.data.has_synced || (!best.data.plain_lyrics.empty() && best.data.plain_lyrics != "Lyrics not found for this track."))) {
            persist_best_candidate(best.candidate);
            return best.data;
        }
    }

    return {"Lyrics not found for this track.", {}, false};
}

std::string LyricsManager::perform_request(const std::string& url) {
    std::string cmd = "curl -s --max-time 6 " + shell_escape(url);
    UniquePipe pipe(popen(cmd.c_str(), "r"));
    if (!pipe) {
        return "";
    }

    std::array<char, 2048> buffer;
    std::string result;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

LyricsData LyricsManager::parse_json_response(const std::string& json) {
    LyricsData data;
    data.has_synced = false;

    auto extract_string_field = [](const std::string& src, const std::string& field_name) -> std::string {
        std::string key = "\"" + field_name + "\"";
        size_t pos = src.find(key);
        if (pos == std::string::npos) return "";

        pos += key.length();
        while (pos < src.length() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r' || src[pos] == '\n')) pos++;
        if (pos >= src.length() || src[pos] != ':') return "";
        pos++; // skip ':'
        while (pos < src.length() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r' || src[pos] == '\n')) pos++;
        if (pos >= src.length() || src[pos] != '"') return "";
        pos++; // skip opening '"'

        std::string result;
        bool escape = false;
        for (size_t i = pos; i < src.length(); ++i) {
            char c = src[i];
            if (escape) {
                if (c == 'n') result += '\n';
                else if (c == 'r') result += '\r';
                else if (c == 't') result += '\t';
                else if (c == '"') result += '"';
                else if (c == '\\') result += '\\';
                else result += c;
                escape = false;
            } else {
                if (c == '\\') {
                    escape = true;
                } else if (c == '"') {
                    break;
                } else {
                    result += c;
                }
            }
        }
        return result;
    };

    std::string plain = extract_string_field(json, "plainLyrics");
    std::string synced = extract_string_field(json, "syncedLyrics");

    if (!plain.empty()) {
        data.plain_lyrics = plain;
    } else {
        data.plain_lyrics = "Lyrics not found for this track.";
    }

    if (!synced.empty()) {
        std::stringstream ss(synced);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.empty()) continue;

            // LRC format: [mm:ss.xx] Text
            size_t bracket_end = line.find(']');
            if (line.front() == '[' && bracket_end != std::string::npos) {
                std::string timestamp_str = line.substr(1, bracket_end - 1);
                std::string text = line.substr(bracket_end + 1);

                while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
                    text.erase(text.begin());
                }

                double timestamp = parse_timestamp(timestamp_str);
                if (timestamp >= 0.0) {
                    data.synced_lyrics.push_back({timestamp, text});
                }
            }
        }

        if (!data.synced_lyrics.empty()) {
            data.has_synced = true;
        }
    }

    return data;
}

double LyricsManager::parse_timestamp(const std::string& timestamp_str) {
    size_t colon_pos = timestamp_str.find(':');
    if (colon_pos == std::string::npos) return -1.0;

    try {
        int minutes = std::stoi(timestamp_str.substr(0, colon_pos));
        double seconds = std::stod(timestamp_str.substr(colon_pos + 1));
        return (minutes * 60.0) + seconds;
    } catch (...) {
        return -1.0;
    }
}

static std::string extract_field(const std::string& src, const std::string& field_name) {
    std::string key = "\"" + field_name + "\"";
    size_t pos = src.find(key);
    if (pos == std::string::npos) return "";

    pos += key.length();
    while (pos < src.length() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r' || src[pos] == '\n')) pos++;
    if (pos >= src.length() || src[pos] != ':') return "";
    pos++; // skip ':'
    while (pos < src.length() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r' || src[pos] == '\n')) pos++;
    if (pos >= src.length() || src[pos] != '"') return "";
    pos++; // skip opening '"'

    std::string result;
    bool escape = false;
    for (size_t i = pos; i < src.length(); ++i) {
        char c = src[i];
        if (escape) {
            if (c == 'n') result += '\n';
            else if (c == 'r') result += '\r';
            else if (c == 't') result += '\t';
            else if (c == '"') result += '"';
            else if (c == '\\') result += '\\';
            else result += c;
            escape = false;
        } else {
            if (c == '\\') {
                escape = true;
            } else if (c == '"') {
                break;
            } else {
                result += c;
            }
        }
    }
    return result;
}

static double extract_number_field(const std::string& src, const std::string& field_name) {
    std::string key = "\"" + field_name + "\":";
    size_t pos = src.find(key);
    if (pos == std::string::npos) return 0.0;

    pos += key.length();
    while (pos < src.length() && (src[pos] == ' ' || src[pos] == '\t')) pos++;
    size_t end = pos;
    while (end < src.length() && (isdigit(static_cast<unsigned char>(src[end])) || src[end] == '.' || src[end] == '-')) {
        end++;
    }
    if (end > pos) {
        return safe_stod(src.substr(pos, end - pos), 0.0);
    }
    return 0.0;
}

std::vector<LyricCandidate> LyricsManager::parse_search_results(const std::string& json) {
    std::vector<LyricCandidate> candidates;
    if (json.empty()) return candidates;

    int brace_count = 0;
    size_t start = std::string::npos;
    bool in_string = false;
    bool escape = false;

    for (size_t i = 0; i < json.length(); ++i) {
        char c = json[i];
        if (escape) {
            escape = false;
            continue;
        }
        if (c == '\\') {
            escape = true;
            continue;
        }
        if (c == '"') {
            in_string = !in_string;
            continue;
        }
        if (!in_string) {
            if (c == '{') {
                if (brace_count == 0) start = i;
                brace_count++;
            } else if (c == '}') {
                brace_count--;
                if (brace_count == 0 && start != std::string::npos) {
                    std::string obj = json.substr(start, i - start + 1);
                    LyricCandidate cand;
                    cand.track_name = extract_field(obj, "trackName");
                    if (cand.track_name.empty()) cand.track_name = extract_field(obj, "name");
                    cand.artist_name = extract_field(obj, "artistName");
                    cand.album_name = extract_field(obj, "albumName");
                    cand.duration = extract_number_field(obj, "duration");
                    cand.plain_lyrics = extract_field(obj, "plainLyrics");
                    cand.synced_lyrics = extract_field(obj, "syncedLyrics");

                    if (!cand.plain_lyrics.empty() || !cand.synced_lyrics.empty()) {
                        candidates.push_back(cand);
                    }
                    start = std::string::npos;
                }
            }
        }
    }
    return candidates;
}

LyricCandidateResult LyricsManager::select_best_candidate(
    const std::vector<LyricCandidate>& candidates,
    const std::string& target_artist,
    const std::string& target_title,
    double target_duration) 
{
    LyricCandidateResult result;
    result.data = {"Lyrics not found for this track.", {}, false};
    result.found = false;

    if (candidates.empty()) return result;

    auto normalize = [](std::string s) -> std::string {
        std::string out;
        for (char c : s) {
            if (isalnum(static_cast<unsigned char>(c)) || c == ' ') {
                out += static_cast<char>(tolower(static_cast<unsigned char>(c)));
            }
        }
        std::string trimmed;
        bool prev_space = true;
        for (char c : out) {
            if (c == ' ') {
                if (!prev_space) trimmed += ' ';
                prev_space = true;
            } else {
                trimmed += c;
                prev_space = false;
            }
        }
        if (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
        return trimmed;
    };

    std::string norm_target_title = normalize(target_title);
    std::string norm_target_artist = normalize(target_artist);

    int best_score = -9999;
    const LyricCandidate* best_cand = nullptr;

    for (const auto& cand : candidates) {
        int score = 0;
        std::string norm_cand_title = normalize(cand.track_name);
        std::string norm_cand_artist = normalize(cand.artist_name);

        // 1. Title match
        if (!norm_target_title.empty()) {
            if (norm_cand_title == norm_target_title) {
                score += 60;
            } else if (norm_cand_title.find(norm_target_title) != std::string::npos ||
                       norm_target_title.find(norm_cand_title) != std::string::npos) {
                score += 30;
            } else {
                score -= 30;
            }
        }

        // 2. Artist match
        if (!norm_target_artist.empty() && norm_target_artist != "unknown") {
            if (norm_cand_artist == norm_target_artist) {
                score += 50;
            } else if (norm_cand_artist.find(norm_target_artist) != std::string::npos ||
                       norm_target_artist.find(norm_cand_artist) != std::string::npos) {
                score += 40;
            } else {
                size_t delim = norm_target_artist.find_first_of(",&");
                if (delim != std::string::npos) {
                    std::string primary = norm_target_artist.substr(0, delim);
                    while (!primary.empty() && primary.back() == ' ') primary.pop_back();
                    if (!primary.empty() && norm_cand_artist.find(primary) != std::string::npos) {
                        score += 35;
                    } else {
                        score -= 35;
                    }
                } else {
                    score -= 35;
                }
            }
        }

        // 3. Duration match
        if (target_duration > 15.0 && cand.duration > 15.0) {
            double diff = std::abs(cand.duration - target_duration);
            if (diff <= 2.5) {
                score += 40;
            } else if (diff <= 6.0) {
                score += 20;
            } else if (diff <= 15.0) {
                score += 5;
            } else if (diff > 45.0) {
                score -= 40;
            }
        }

        // 4. Synced lyrics preference
        if (!cand.synced_lyrics.empty()) {
            score += 15;
        }

        if (score > best_score) {
            best_score = score;
            best_cand = &cand;
        }
    }

    int min_required_score = (norm_target_artist.empty() || norm_target_artist == "unknown") ? 90 : 25;
    if (best_cand && best_score >= min_required_score) {
        LyricsData data;
        data.plain_lyrics = best_cand->plain_lyrics.empty() ? "Lyrics not found for this track." : best_cand->plain_lyrics;
        data.has_synced = false;

        if (!best_cand->synced_lyrics.empty()) {
            std::stringstream ss(best_cand->synced_lyrics);
            std::string line;
            while (std::getline(ss, line)) {
                if (line.empty()) continue;
                size_t bracket_end = line.find(']');
                if (line.front() == '[' && bracket_end != std::string::npos) {
                    std::string timestamp_str = line.substr(1, bracket_end - 1);
                    std::string text = line.substr(bracket_end + 1);
                    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.erase(text.begin());
                    double timestamp = parse_timestamp(timestamp_str);
                    if (timestamp >= 0.0) {
                        data.synced_lyrics.push_back({timestamp, text});
                    }
                }
            }
            if (!data.synced_lyrics.empty()) {
                data.has_synced = true;
            }
        }
        result.data = data;
        result.candidate = *best_cand;
        result.found = true;
        return result;
    }

    return result;
}
