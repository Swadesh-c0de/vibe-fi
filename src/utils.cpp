#include "utils.hpp"
#include <regex>
#include <array>
#include <memory>
#include <stdexcept>
#include <iostream>

bool is_url(const std::string& path) {
    std::regex url_regex(R"(^(http|https)://)");
    return std::regex_search(path, url_regex);
}

std::string get_youtube_stream_url(const std::string& url) {
    if (!is_url(url)) return url;
    
    std::string result;
    // Use a slightly more robust command
    std::string cmd = "yt-dlp --no-progress -f bestaudio -g \"" + url + "\" 2>/dev/null";
    
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        return "";
    }
    
    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        result = buffer;
    }
    
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    
    if (result.empty()) {
        // Fallback to the URL itself if extraction fails, mpv might handle it via ytdl hook
        return url;
    }
    
    return result;
}

std::string get_audio_duration(const std::string& path) {
    std::string cmd = "ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"" + path + "\" 2>/dev/null";
    std::array<char, 128> buffer;
    std::string result;
    
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        return "";
    }
    
    if (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result = buffer.data();
    }
    
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    
    try {
        double seconds = std::stod(result);
        return format_duration(seconds);
    } catch (...) {
        return "";
    }
}

std::string format_duration(double seconds) {
    int total_seconds = static_cast<int>(seconds);
    int minutes = total_seconds / 60;
    int secs = total_seconds % 60;
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d:%02d", minutes, secs);
    return std::string(buffer);
}

std::string sanitize_text(const std::string& text) {
    std::string result;
    for (char c : text) {
        // Allow printable ASCII characters
        if (isprint(static_cast<unsigned char>(c))) {
            result += c;
        }
    }
    return result;
}

