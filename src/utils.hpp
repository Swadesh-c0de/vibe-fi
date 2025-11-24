#ifndef UTILS_HPP
#define UTILS_HPP

#include <vector>
#include <map>
#include <string>



bool is_url(const std::string& path);
std::string get_youtube_stream_url(const std::string& url);
std::string get_audio_duration(const std::string& path);
std::string format_duration(double seconds);
std::string sanitize_text(const std::string& text);

#endif // UTILS_HPP
