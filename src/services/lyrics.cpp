#include "lyrics.hpp"
#include "utils.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <array>

namespace fs = std::filesystem;

LyricsManager::LyricsManager() {}

LyricsData LyricsManager::fetch_lyrics(const std::string& artist, const std::string& title) {
    if (title.empty()) {
        return {"Song title missing.", {}, false};
    }

    auto url_encode = [](const std::string& value) {
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
    };

    std::string safe_artist = url_encode(artist.empty() ? "Unknown" : artist);
    std::string safe_title = url_encode(title);

    std::string cache_dir = get_vibe_dir() + "/cache/lyrics/";
    std::error_code ec;
    fs::create_directories(cache_dir, ec);
    std::string cache_file = cache_dir + safe_artist + "_" + safe_title + ".json";

    // Check local cache
    if (fs::exists(cache_file, ec)) {
        std::ifstream in(cache_file);
        if (in.is_open()) {
            std::stringstream buffer;
            buffer << in.rdbuf();
            std::string cached_json = buffer.str();
            if (!cached_json.empty() && cached_json.find("\"error\"") == std::string::npos) {
                return parse_json_response(cached_json);
            }
        }
    }

    // If not cached and offline, do not stall on network requests
    if (!is_online()) {
        return {"Internet connection issue: Cannot fetch lyrics offline.", {}, false};
    }

    std::string response;

    // 1. Direct query with artist & title if artist is available
    if (!artist.empty() && artist != "Unknown") {
        std::string url = "https://lrclib.net/api/get?artist_name=" + safe_artist + "&track_name=" + safe_title;
        response = perform_request(url);
    }

    // 2. Fallback: Search endpoint by title / combined query if direct get failed
    if (response.empty() || response.find("\"error\"") != std::string::npos || response.find("404") != std::string::npos) {
        std::string query_str = artist.empty() ? title : (artist + " " + title);
        std::string url = "https://lrclib.net/api/search?q=" + url_encode(query_str);
        response = perform_request(url);
    }

    if (response.empty() || response.find("\"error\"") != std::string::npos) {
        return {"No lyrics found online.", {}, false};
    }

    LyricsData parsed = parse_json_response(response);
    if (parsed.has_synced || !parsed.plain_lyrics.empty()) {
        // Save valid response to cache
        std::ofstream out(cache_file);
        if (out.is_open()) {
            out << response;
        }
    }

    return parsed;
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
        std::string key = "\"" + field_name + "\":\"";
        size_t pos = src.find(key);
        if (pos == std::string::npos) return "";

        pos += key.length();
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
