#include "utils.hpp"
#include <regex>
#include <array>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <unistd.h>

namespace fs = std::filesystem;

std::string shell_escape(const std::string& arg) {
    std::string escaped = "'";
    for (char c : arg) {
        if (c == '\'') {
            escaped += "'\\''";
        } else {
            escaped += c;
        }
    }
    escaped += "'";
    return escaped;
}

std::string find_executable(const std::string& name) {
    const char* home = getenv("HOME");
    std::vector<std::string> candidates;

    // Check standard locations first
    candidates.push_back("/usr/local/bin/" + name);
    candidates.push_back("/usr/bin/" + name);
    candidates.push_back("/opt/homebrew/bin/" + name);
    candidates.push_back("/bin/" + name);
    if (home) {
        candidates.push_back(std::string(home) + "/.local/bin/" + name);
    }

    // Check PATH
    const char* path_env = getenv("PATH");
    if (path_env) {
        std::stringstream ss(path_env);
        std::string dir;
        while (std::getline(ss, dir, ':')) {
            if (!dir.empty()) {
                candidates.push_back(dir + "/" + name);
            }
        }
    }

    for (const auto& path : candidates) {
        if (fs::exists(path) && access(path.c_str(), X_OK) == 0) {
            return path;
        }
    }

    // Fallback to name if not found specifically
    return name;
}

bool is_url(const std::string& path) {
    static const std::regex url_regex(R"(^(http|https)://)", std::regex::optimize);
    return std::regex_search(path, url_regex);
}

std::string get_youtube_stream_url(const std::string& url) {
    if (!is_url(url)) return url;

    std::string ytdl_path = find_executable("yt-dlp");
    std::string cmd = shell_escape(ytdl_path) + " --no-progress -f bestaudio -g " + shell_escape(url) + " 2>/dev/null";

    UniquePipe pipe(popen(cmd.c_str(), "r"));
    if (!pipe) {
        return url;
    }

    char buffer[1024];
    std::string result;
    if (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        result = buffer;
    }

    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    if (!result.empty() && result.back() == '\r') {
        result.pop_back();
    }

    return result.empty() ? url : result;
}

std::string get_audio_duration(const std::string& path) {
    std::string ffprobe_path = find_executable("ffprobe");
    std::string cmd = shell_escape(ffprobe_path) + " -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 " + shell_escape(path) + " 2>/dev/null";

    UniquePipe pipe(popen(cmd.c_str(), "r"));
    if (!pipe) {
        return "";
    }

    std::array<char, 128> buffer;
    std::string result;
    if (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result = buffer.data();
    }

    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    if (!result.empty() && result.back() == '\r') {
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
    if (seconds < 0) seconds = 0;
    int total_seconds = static_cast<int>(seconds);
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int secs = total_seconds % 60;

    char buffer[32];
    if (hours > 0) {
        snprintf(buffer, sizeof(buffer), "%d:%02d:%02d", hours, minutes, secs);
    } else {
        snprintf(buffer, sizeof(buffer), "%d:%02d", minutes, secs);
    }
    return std::string(buffer);
}

std::string sanitize_text(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (unsigned char c : text) {
        // Allow printable ASCII and valid UTF-8 multibyte bytes (>= 128).
        // Only strip ASCII control characters (0-31 and 127).
        if (c >= 32 && c != 127) {
            result += static_cast<char>(c);
        }
    }
    return result;
}

std::string get_vibe_dir() {
    const char* home = getenv("HOME");
    std::string dir = home ? (std::string(home) + "/.vibe-fi") : ".vibe-fi";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

double safe_stod(const std::string& s, double default_val) {
    if (s.empty()) return default_val;
    try {
        return std::stod(s);
    } catch (...) {
        return default_val;
    }
}

float safe_stof(const std::string& s, float default_val) {
    if (s.empty()) return default_val;
    try {
        return std::stof(s);
    } catch (...) {
        return default_val;
    }
}

int safe_stoi(const std::string& s, int default_val) {
    if (s.empty()) return default_val;
    try {
        return std::stoi(s);
    } catch (...) {
        return default_val;
    }
}
