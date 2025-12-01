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
    std::string result;
    // Added --force-ipv4 to help with network issues and --no-progress to avoid escape sequences
    // Removed 2>/dev/null to allow error messages to be seen in the console
    std::string cmd = "yt-dlp --no-progress --force-ipv4 -g -f bestaudio \"" + url + "\"";
    
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    
    // Read character by character to handle URLs of any length
    int c;
    while ((c = fgetc(pipe.get())) != EOF) {
        result += static_cast<char>(c);
    }
    
    // Remove newline at the end
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    
    if (result.empty()) {
        throw std::runtime_error("Failed to extract stream URL");
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
