#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>
#include <memory>
#include <cstdio>

// Custom deleter for popen FILE pointers to avoid compiler warnings
struct PipeCloser {
    void operator()(FILE* fp) const {
        if (fp) {
            pclose(fp);
        }
    }
};

using UniquePipe = std::unique_ptr<FILE, PipeCloser>;

// Shell command escaping and executable search
std::string shell_escape(const std::string& arg);
std::string find_executable(const std::string& name);

// URL and stream helpers
struct StreamInfo {
    std::string stream_url;
    std::string title;
    std::string artist;
    double duration = 0.0;
};

bool is_url(const std::string& path);
bool is_online(int timeout_ms = 1500);
StreamInfo resolve_stream_info(const std::string& url);
std::string get_youtube_stream_url(const std::string& url);
std::string get_audio_duration(const std::string& path);
std::string format_duration(double seconds);
double parse_duration_to_seconds(const std::string& dur_str);

// Text and path sanitizers
std::string clean_song_title(const std::string& title);
void parse_artist_and_title(const std::string& raw_title, const std::string& raw_artist, std::string& out_artist, std::string& out_title, const std::string& context_hint = "");
std::string sanitize_text(const std::string& text);
std::string get_vibe_dir();

// Bottle dependency isolation helpers
struct BottleManifest {
    std::string bottle_version;
    std::string bottle_dir;
    std::string created_at;
    std::string platform;
    std::string package_manager;
    std::vector<std::string> bottled_binaries;
    std::vector<std::string> installed_system_packages;
    std::vector<std::string> preinstalled_dependencies;
    bool exists = false;
};

std::string get_bottle_dir();
std::string get_bottle_bin_dir();
bool is_bottle_active();
BottleManifest read_bottle_manifest();
bool ensure_bottled_ytdlp();
void print_bottle_status();

// Safe numeric conversions
double safe_stod(const std::string& s, double default_val = 0.0);
float safe_stof(const std::string& s, float default_val = 0.0f);
int safe_stoi(const std::string& s, int default_val = 0);
int64_t safe_stoll(const std::string& s, int64_t default_val = 0);

#endif // UTILS_HPP
