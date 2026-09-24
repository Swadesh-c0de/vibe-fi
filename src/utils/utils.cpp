#include "utils.hpp"
#include <regex>
#include <array>
#include <sstream>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <chrono>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>

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

    // 1. FIRST PRIORITY: Vibe-Fi Isolated Bottle
    const char* bottle_env = getenv("VIBE_BOTTLE_DIR");
    if (bottle_env && bottle_env[0] != '\0') {
        candidates.push_back(std::string(bottle_env) + "/bin/" + name);
    }
    std::string default_bottle = (home ? (std::string(home) + "/.vibe-fi/bottle/bin/") : ".vibe-fi/bottle/bin/") + name;
    candidates.push_back(default_bottle);

    // 2. Standard system locations
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

bool is_online(int timeout_ms) {
    static std::chrono::steady_clock::time_point last_check{};
    static bool cached_status = false;

    auto now = std::chrono::steady_clock::now();
    if (last_check.time_since_epoch().count() > 0 &&
        std::chrono::duration_cast<std::chrono::seconds>(now - last_check).count() < 3) {
        return cached_status;
    }

    struct Endpoint {
        const char* ip;
        int port;
    };
    const Endpoint endpoints[] = {
        {"1.1.1.1", 443}, // Cloudflare HTTPS (universally open)
        {"8.8.8.8", 53},  // Google DNS
        {"1.1.1.1", 53}   // Cloudflare DNS
    };
    bool connected = false;

    for (const auto& ep : endpoints) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;

        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(ep.port);
        inet_pton(AF_INET, ep.ip, &addr.sin_addr);

        int res = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
        if (res == 0) {
            close(sock);
            connected = true;
            break;
        }

        if (errno == EINPROGRESS) {
            struct pollfd pfd;
            pfd.fd = sock;
            pfd.events = POLLOUT;

            int poll_res = poll(&pfd, 1, timeout_ms);
            if (poll_res > 0) {
                int err = 0;
                socklen_t len = sizeof(err);
                if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len) == 0 && err == 0) {
                    connected = true;
                }
            }
        }

        close(sock);
        if (connected) break;
    }

    cached_status = connected;
    last_check = now;
    return connected;
}

StreamInfo resolve_stream_info(const std::string& url) {
    StreamInfo info;
    info.stream_url = url;
    if (!is_url(url)) {
        info.title = url;
        return info;
    }

    std::string ytdl_path = find_executable("yt-dlp");
    if (ytdl_path.empty()) {
        return info;
    }

    std::string cmd = shell_escape(ytdl_path) + 
                      " --no-progress -f bestaudio --no-warnings --print \"%(title)s|%(uploader)s|%(artist)s|%(duration)s\" -g " + 
                      shell_escape(url) + " 2>/dev/null";

    UniquePipe pipe(popen(cmd.c_str(), "r"));
    if (!pipe) {
        return info;
    }

    char buffer[2048];
    std::string line1, line2;
    if (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        line1 = buffer;
    }
    if (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        line2 = buffer;
    }

    auto trim_line = [](std::string& s) {
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    };
    trim_line(line1);
    trim_line(line2);

    if (!line2.empty() && is_url(line2)) {
        info.stream_url = line2;
    } else if (!line1.empty() && is_url(line1)) {
        info.stream_url = line1;
    }

    if (!line1.empty() && line1 != info.stream_url) {
        std::vector<std::string> parts;
        std::stringstream ss(line1);
        std::string part;
        while (std::getline(ss, part, '|')) {
            parts.push_back(part);
        }

        if (parts.size() >= 1 && parts[0] != "NA") info.title = sanitize_text(parts[0]);
        std::string uploader = (parts.size() >= 2 && parts[1] != "NA") ? sanitize_text(parts[1]) : "";
        std::string artist = (parts.size() >= 3 && parts[2] != "NA") ? sanitize_text(parts[2]) : "";
        if (parts.size() >= 4 && parts[3] != "NA") info.duration = safe_stod(parts[3], 0.0);

        if (!artist.empty()) {
            info.artist = artist;
        } else if (!uploader.empty()) {
            info.artist = uploader;
        }

        if (info.artist.length() > 8 && info.artist.substr(info.artist.length() - 8) == " - Topic") {
            info.artist = info.artist.substr(0, info.artist.length() - 8);
        }
    }

    return info;
}

std::string get_youtube_stream_url(const std::string& url) {
    return resolve_stream_info(url).stream_url;
}

std::string clean_song_title(const std::string& raw) {
    std::string s = raw;
    
    // Remove bracketed expressions that contain common noise keywords
    auto strip_noise_brackets = [](std::string text) -> std::string {
        std::string result;
        size_t i = 0;
        while (i < text.length()) {
            char open_ch = text[i];
            char close_ch = 0;
            if (open_ch == '(') close_ch = ')';
            else if (open_ch == '[') close_ch = ']';

            if (close_ch != 0) {
                size_t close_pos = text.find(close_ch, i + 1);
                if (close_pos != std::string::npos) {
                    std::string inside = text.substr(i + 1, close_pos - i - 1);
                    std::string lower_inside = inside;
                    for (char& c : lower_inside) c = tolower(static_cast<unsigned char>(c));

                    bool is_noise = (lower_inside.find("lyric") != std::string::npos ||
                                     lower_inside.find("official") != std::string::npos ||
                                     lower_inside.find("video") != std::string::npos ||
                                     lower_inside.find("audio") != std::string::npos ||
                                     lower_inside.find("visualizer") != std::string::npos ||
                                     lower_inside.find("hd") != std::string::npos ||
                                     lower_inside.find("4k") != std::string::npos ||
                                     lower_inside.find("remaster") != std::string::npos ||
                                     lower_inside.find("mv") != std::string::npos ||
                                     lower_inside.find("prod.") != std::string::npos ||
                                     lower_inside.find("full song") != std::string::npos ||
                                     lower_inside.find("full video") != std::string::npos);

                    if (is_noise) {
                        i = close_pos + 1;
                        continue;
                    }
                }
            }
            result += text[i];
            i++;
        }
        return result;
    };

    s = strip_noise_brackets(s);

    // Strip trailing pipe / slash notes (e.g. "| Official Video" or "// 4K")
    size_t pipe_pos = s.find_first_of("|/\\");
    if (pipe_pos != std::string::npos && pipe_pos > 2) {
        std::string suffix = s.substr(pipe_pos);
        std::string lower_suf = suffix;
        for (char& c : lower_suf) c = tolower(static_cast<unsigned char>(c));
        if (lower_suf.find("video") != std::string::npos ||
            lower_suf.find("lyric") != std::string::npos ||
            lower_suf.find("audio") != std::string::npos ||
            lower_suf.find("official") != std::string::npos) {
            s = s.substr(0, pipe_pos);
        }
    }

    // Trim whitespace
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();

    return s;
}

void parse_artist_and_title(const std::string& raw_title, const std::string& raw_artist, std::string& out_artist, std::string& out_title, const std::string& context_hint) {
    std::string cleaned = clean_song_title(raw_title);

    // Check for "Artist - Title" format
    size_t dash_pos = cleaned.find(" - ");
    if (dash_pos != std::string::npos) {
        out_artist = cleaned.substr(0, dash_pos);
        out_title = cleaned.substr(dash_pos + 3);
    } else {
        out_title = cleaned;
        out_artist = raw_artist;
    }

    auto format_camel = [](const std::string& in) -> std::string {
        std::string out;
        for (size_t i = 0; i < in.length(); ++i) {
            if (i > 0 && islower(static_cast<unsigned char>(in[i-1])) && isupper(static_cast<unsigned char>(in[i]))) {
                out += ' ';
            }
            out += in[i];
        }
        return out;
    };

    // If out_artist is empty or "Unknown", and context_hint is provided (e.g. playlist name or query)
    if ((out_artist.empty() || out_artist == "Unknown") && !context_hint.empty()) {
        std::string lower_hint = context_hint;
        for (char& c : lower_hint) c = tolower(static_cast<unsigned char>(c));
        if (lower_hint != "favorites" && lower_hint != "queue" && lower_hint != "playlist" && 
            lower_hint != "default" && lower_hint != "music" && lower_hint != "tracks") {
            out_artist = format_camel(context_hint);
        }
    } else if (!out_artist.empty() && out_artist != "Unknown" && out_artist.find(' ') == std::string::npos) {
        // If out_artist is a single-word camelCase string (e.g. "AlanWalker"), split to "Alan Walker"
        out_artist = format_camel(out_artist);
    }

    // Remove feat/ft in title if present
    auto strip_feat = [](std::string text) -> std::string {
        std::string lower = text;
        for (char& c : lower) c = tolower(static_cast<unsigned char>(c));

        size_t feat_pos = lower.find(" ft. ");
        if (feat_pos == std::string::npos) feat_pos = lower.find(" feat. ");
        if (feat_pos == std::string::npos) feat_pos = lower.find(" ft ");
        if (feat_pos == std::string::npos) feat_pos = lower.find(" feat ");
        if (feat_pos == std::string::npos) feat_pos = lower.find(" featuring ");

        if (feat_pos != std::string::npos) {
            return text.substr(0, feat_pos);
        }
        return text;
    };
    out_title = strip_feat(out_title);

    // Clean artist if topic or vevo
    if (out_artist.length() > 8 && out_artist.substr(out_artist.length() - 8) == " - Topic") {
        out_artist = out_artist.substr(0, out_artist.length() - 8);
    }
    if (out_artist.length() > 4 && out_artist.substr(out_artist.length() - 4) == "VEVO") {
        out_artist = out_artist.substr(0, out_artist.length() - 4);
    }

    // Trim both
    while (!out_title.empty() && (out_title.front() == ' ' || out_title.front() == '\t')) out_title.erase(out_title.begin());
    while (!out_title.empty() && (out_title.back() == ' ' || out_title.back() == '\t')) out_title.pop_back();

    while (!out_artist.empty() && (out_artist.front() == ' ' || out_artist.front() == '\t')) out_artist.erase(out_artist.begin());
    while (!out_artist.empty() && (out_artist.back() == ' ' || out_artist.back() == '\t')) out_artist.pop_back();
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

    double seconds = safe_stod(result, 0.0);
    if (seconds > 0.0) {
        return format_duration(seconds);
    }
    return "";
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

double parse_duration_to_seconds(const std::string& dur_str) {
    if (dur_str.empty()) return 0.0;
    try {
        if (dur_str.find(':') != std::string::npos) {
            std::stringstream ss(dur_str);
            std::string segment;
            std::vector<double> parts;
            while (std::getline(ss, segment, ':')) {
                while (!segment.empty() && (segment.front() == ' ' || segment.front() == '\t')) segment.erase(segment.begin());
                while (!segment.empty() && (segment.back() == ' ' || segment.back() == '\t')) segment.pop_back();
                if (!segment.empty()) {
                    parts.push_back(safe_stod(segment, 0.0));
                }
            }
            if (parts.size() == 2) {
                return parts[0] * 60.0 + parts[1];
            } else if (parts.size() == 3) {
                return parts[0] * 3600.0 + parts[1] * 60.0 + parts[2];
            }
        }
        return safe_stod(dur_str, 0.0);
    } catch (...) {
        return 0.0;
    }
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

std::string get_bottle_dir() {
    const char* env_dir = getenv("VIBE_BOTTLE_DIR");
    if (env_dir && env_dir[0] != '\0') {
        return env_dir;
    }
    return get_vibe_dir() + "/bottle";
}

std::string get_bottle_bin_dir() {
    return get_bottle_dir() + "/bin";
}

bool is_bottle_active() {
    std::error_code ec;
    return fs::exists(get_bottle_dir(), ec) && fs::is_directory(get_bottle_dir(), ec);
}

static std::vector<std::string> extract_json_string_array(const std::string& json, const std::string& key) {
    std::vector<std::string> items;
    size_t kpos = json.find("\"" + key + "\"");
    if (kpos == std::string::npos) return items;

    size_t open_bracket = json.find('[', kpos);
    if (open_bracket == std::string::npos) return items;

    size_t close_bracket = json.find(']', open_bracket);
    if (close_bracket == std::string::npos) return items;

    std::string array_str = json.substr(open_bracket + 1, close_bracket - open_bracket - 1);
    bool in_str = false;
    std::string current;
    for (size_t i = 0; i < array_str.size(); ++i) {
        char c = array_str[i];
        if (c == '"' && (i == 0 || array_str[i - 1] != '\\')) {
            if (in_str) {
                if (!current.empty()) {
                    items.push_back(current);
                    current.clear();
                }
                in_str = false;
            } else {
                in_str = true;
            }
        } else if (in_str) {
            current += c;
        }
    }
    return items;
}

static std::string extract_json_string_value(const std::string& json, const std::string& key) {
    size_t kpos = json.find("\"" + key + "\"");
    if (kpos == std::string::npos) return "";

    size_t colon = json.find(':', kpos);
    if (colon == std::string::npos) return "";

    size_t first_quote = json.find('"', colon);
    if (first_quote == std::string::npos) return "";

    size_t second_quote = json.find('"', first_quote + 1);
    if (second_quote == std::string::npos) return "";

    return json.substr(first_quote + 1, second_quote - first_quote - 1);
}

BottleManifest read_bottle_manifest() {
    BottleManifest manifest;
    std::string manifest_path = get_bottle_dir() + "/manifest.json";
    std::ifstream file(manifest_path);
    if (!file.is_open()) {
        manifest.exists = false;
        return manifest;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    manifest.bottle_version = extract_json_string_value(content, "bottle_version");
    manifest.bottle_dir = extract_json_string_value(content, "bottle_dir");
    manifest.created_at = extract_json_string_value(content, "created_at");
    manifest.platform = extract_json_string_value(content, "platform");
    manifest.package_manager = extract_json_string_value(content, "package_manager");
    manifest.bottled_binaries = extract_json_string_array(content, "bottled_binaries");
    manifest.installed_system_packages = extract_json_string_array(content, "installed_system_packages");
    manifest.preinstalled_dependencies = extract_json_string_array(content, "preinstalled_dependencies");
    manifest.exists = true;

    return manifest;
}

bool ensure_bottled_ytdlp() {
    std::string current_ytdl = find_executable("yt-dlp");
    if (current_ytdl != "yt-dlp" && fs::exists(current_ytdl)) {
        return true;
    }

    std::string bbin = get_bottle_bin_dir();
    std::error_code ec;
    fs::create_directories(bbin, ec);

    std::string target_bin = bbin + "/yt-dlp";
    std::cout << ":: Downloading isolated yt-dlp to Vibe Bottle (" << target_bin << ")...\n";
    std::string download_cmd = "curl -sL https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp -o " + shell_escape(target_bin) + " && chmod a+rx " + shell_escape(target_bin);
    int ret = std::system(download_cmd.c_str());
    return (ret == 0 && fs::exists(target_bin) && access(target_bin.c_str(), X_OK) == 0);
}

void print_bottle_status() {
    std::string bdir = get_bottle_dir();
    std::string bbin = get_bottle_bin_dir();
    auto manifest = read_bottle_manifest();

    std::cout << "\n\033[1;36m─────────────────────────────────────────────────────────────\033[0m\n";
    std::cout << "  \033[1;37mVibe-Fi Bottle Runtime Environment\033[0m\n";
    std::cout << "\033[1;36m─────────────────────────────────────────────────────────────\033[0m\n";
    std::cout << "Bottle Directory:     \033[0;36m" << bdir << "\033[0m\n";
    std::cout << "Bottle Binaries Dir:  \033[0;36m" << bbin << "\033[0m\n";

    if (manifest.exists) {
        std::cout << "Status:               \033[1;32mActive (Configured via Bottle)\033[0m\n";
        std::cout << "Platform:             " << (manifest.platform.empty() ? "Unknown" : manifest.platform) << "\n";
        std::cout << "Package Manager:      " << (manifest.package_manager.empty() ? "None" : manifest.package_manager) << "\n";
        if (!manifest.created_at.empty()) {
            std::cout << "Bottle Created:       " << manifest.created_at << "\n";
        }
        std::cout << "\n\033[1;33mBottled Standalone Binaries (Isolated to Vibe-Fi):\033[0m\n";
        if (manifest.bottled_binaries.empty()) {
            std::cout << "  (none - all standalone tools are using host preinstalled binaries)\n";
        } else {
            for (const auto& b : manifest.bottled_binaries) {
                std::string full_path = bbin + "/" + b;
                bool exists = fs::exists(full_path);
                std::cout << "  -> \033[0;32m" << b << "\033[0m (" << (exists ? "ready" : "missing") << ": " << full_path << ")\n";
            }
        }

        std::cout << "\n\033[1;33mSystem Packages Installed for Vibe-Fi (Tracked for Clean Removal):\033[0m\n";
        if (manifest.installed_system_packages.empty()) {
            std::cout << "  (none - all system libraries were already preinstalled on host)\n";
        } else {
            for (const auto& pkg : manifest.installed_system_packages) {
                std::cout << "  -> \033[0;36m" << pkg << "\033[0m\n";
            }
        }

        if (!manifest.preinstalled_dependencies.empty()) {
            std::cout << "\n\033[1;33mProtected Host Preinstalled Dependencies (Never Uninstalled):\033[0m\n";
            for (const auto& p : manifest.preinstalled_dependencies) {
                std::cout << "  -> \033[0;37m" << p << "\033[0m [host preinstalled]\n";
            }
        }
    } else {
        std::cout << "Status:               \033[1;33mHost-Native / Unbottled (no bottle manifest)\033[0m\n";
        std::cout << "\nTool Detection in PATH:\n";
        std::string ytdl = find_executable("yt-dlp");
        std::string ffmpeg = find_executable("ffmpeg");
        std::cout << "  yt-dlp:  " << (ytdl == "yt-dlp" ? "not found" : ytdl) << "\n";
        std::cout << "  ffmpeg:  " << (ffmpeg == "ffmpeg" ? "not found" : ffmpeg) << "\n";
    }

    std::cout << "\n\033[1;36m─────────────────────────────────────────────────────────────\033[0m\n\n";
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

int64_t safe_stoll(const std::string& s, int64_t default_val) {
    if (s.empty()) return default_val;
    try {
        return std::stoll(s);
    } catch (...) {
        return default_val;
    }
}
