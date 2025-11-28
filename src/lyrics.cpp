#include "lyrics.hpp"
#include <iostream>
#include <memory>
#include <array>
#include <regex>
#include <algorithm>

LyricsManager::LyricsManager() {}

LyricsData LyricsManager::fetch_lyrics(const std::string& artist, const std::string& title) {
    if (artist.empty() || title.empty()) {
        return {"Artist or title missing.", {}, false};
    }

    // Simple URL encoding (basic)
    auto url_encode = [](const std::string& value) {
        std::string escaped;
        escaped.reserve(value.length());
        for (char c : value) {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                escaped += c;
            } else {
                char buf[4];
                snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
                escaped += buf;
            }
        }
        return escaped;
    };

    std::string url = "https://lrclib.net/api/get?artist_name=" + url_encode(artist) + "&track_name=" + url_encode(title);
    std::string response = perform_request(url);
    
    if (response.empty()) {
        return {"No lyrics found or network error.", {}, false};
    }

    return parse_json_response(response);
}

std::string LyricsManager::perform_request(const std::string& url) {
    std::string cmd = "curl -s --max-time 10 \"" + url + "\"";
    std::array<char, 128> buffer;
    std::string result;
    
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        return "";
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    
    return result;
}

LyricsData LyricsManager::parse_json_response(const std::string& json) {
    LyricsData data;
    data.has_synced = false;

    // Look for "plainLyrics":"
    std::string plain_key = "\"plainLyrics\":\"";
    size_t pos = json.find(plain_key);
    if (pos != std::string::npos) {
        pos += plain_key.length();
        std::string lyrics;
        bool escape = false;
        for (size_t i = pos; i < json.length(); ++i) {
            char c = json[i];
            if (escape) {
                if (c == 'n') lyrics += '\n';
                else if (c == 'r') lyrics += '\r';
                else if (c == 't') lyrics += '\t';
                else if (c == '"') lyrics += '"';
                else if (c == '\\') lyrics += '\\';
                else lyrics += c;
                escape = false;
            } else {
                if (c == '\\') escape = true;
                else if (c == '"') break;
                else lyrics += c;
            }
        }
        data.plain_lyrics = lyrics;
    } else {
        data.plain_lyrics = "Lyrics not found in response.";
    }

    // Look for "syncedLyrics":"
    std::string synced_key = "\"syncedLyrics\":\"";
    pos = json.find(synced_key);
    if (pos != std::string::npos) {
        pos += synced_key.length();
        std::string lyrics;
        bool escape = false;
        for (size_t i = pos; i < json.length(); ++i) {
            char c = json[i];
            if (escape) {
                if (c == 'n') lyrics += '\n';
                else if (c == 'r') lyrics += '\r';
                else if (c == 't') lyrics += '\t';
                else if (c == '"') lyrics += '"';
                else if (c == '\\') lyrics += '\\';
                else lyrics += c;
                escape = false;
            } else {
                if (c == '\\') escape = true;
                else if (c == '"') break;
                else lyrics += c;
            }
        }
        
        // Parse synced lyrics
        if (!lyrics.empty()) {
            std::stringstream ss(lyrics);
            std::string line;
            while (std::getline(ss, line)) {
                if (line.empty()) continue;
                
                // Format: [mm:ss.xx] Text
                size_t bracket_end = line.find(']');
                if (line.front() == '[' && bracket_end != std::string::npos) {
                    std::string timestamp_str = line.substr(1, bracket_end - 1);
                    std::string text = line.substr(bracket_end + 1);
                    
                    // Trim leading space from text
                    if (!text.empty() && text.front() == ' ') {
                        text = text.substr(1);
                    }
                    
                    double timestamp = parse_timestamp(timestamp_str);
                    if (timestamp >= 0) {
                        data.synced_lyrics.push_back({timestamp, text});
                    }
                }
            }
            if (!data.synced_lyrics.empty()) {
                data.has_synced = true;
            }
        }
    }

    return data;
}

double LyricsManager::parse_timestamp(const std::string& timestamp_str) {
    // mm:ss.xx
    size_t colon_pos = timestamp_str.find(':');
    if (colon_pos == std::string::npos) return -1.0;
    
    try {
        int minutes = std::stoi(timestamp_str.substr(0, colon_pos));
        double seconds = std::stod(timestamp_str.substr(colon_pos + 1));
        return minutes * 60.0 + seconds;
    } catch (...) {
        return -1.0;
    }
}
