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
bool is_url(const std::string& path);
std::string get_youtube_stream_url(const std::string& url);
std::string get_audio_duration(const std::string& path);
std::string format_duration(double seconds);

// Text and path sanitizers
std::string sanitize_text(const std::string& text);
std::string get_vibe_dir();

// Safe numeric conversions
double safe_stod(const std::string& s, double default_val = 0.0);
float safe_stof(const std::string& s, float default_val = 0.0f);
int safe_stoi(const std::string& s, int default_val = 0);

#endif // UTILS_HPP
