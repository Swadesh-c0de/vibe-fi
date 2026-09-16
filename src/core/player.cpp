#include "player.hpp"
#include "utils.hpp"
#include <stdexcept>
#include <iostream>

#include <clocale>
#include <cstring>

Player::Player() : mpv(nullptr) {
    std::setlocale(LC_NUMERIC, "C"); // libmpv requires LC_NUMERIC="C" to parse decimal points reliably
    mpv = mpv_create();
    if (!mpv) {
        throw std::runtime_error("Failed to create libmpv context");
    }

    // Configure mpv defaults for optimal, resilient audio streaming
    check_error(mpv_set_option_string(mpv, "vo", "null"));                   // Audio only, disable video window
    check_error(mpv_set_option_string(mpv, "ytdl", "yes"));                  // Enable YouTube extraction
    check_error(mpv_set_option_string(mpv, "ytdl-format", "bestaudio[ext=m4a]/bestaudio[ext=webm]/bestaudio/best"));
    check_error(mpv_set_option_string(mpv, "audio-display", "no"));          // Don't render embedded album art as video

    // Network resilience: auto-reconnect streamed audio on network hiccups, buffer up to 32MB ahead
    mpv_set_option_string(mpv, "stream-lavf-o", "reconnect=1,reconnect_streamed=1,reconnect_delay_max=5");
    mpv_set_option_string(mpv, "network-timeout", "30");
    mpv_set_option_string(mpv, "demuxer-max-bytes", "32MiB");
    mpv_set_option_string(mpv, "demuxer-readahead-secs", "60");
    mpv_set_option_string(mpv, "ytdl-raw-options", "no-check-certificates=,retries=3,socket-timeout=15");
    mpv_set_option_string(mpv, "user-agent", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36");

    // Dynamically locate yt-dlp across PATH and common install directories
    std::string ytdl_path = find_executable("yt-dlp");
    if (!ytdl_path.empty()) {
        std::string script_opt = "ytdl_hook-ytdl_path=" + ytdl_path;
        mpv_set_option_string(mpv, "script-opts", script_opt.c_str());
    }

    // Attach real-time audio statistics filter for cross-platform beat & loudness analysis
    mpv_set_option_string(mpv, "af", "@astats:lavfi=[astats=metadata=1:reset=1:length=0.04]");

    check_error(mpv_initialize(mpv));
}

Player::~Player() {
    if (mpv) {
        mpv_terminate_destroy(mpv);
        mpv = nullptr;
    }
}

void Player::check_error(int status) {
    if (status < 0) {
        throw std::runtime_error(std::string("mpv error: ") + mpv_error_string(status));
    }
}

void Player::load(const std::string& path, const std::string& mode) {
    const char* cmd[] = {"loadfile", path.c_str(), mode.c_str(), nullptr};
    check_error(mpv_command(mpv, cmd));
}

void Player::play() {
    int flag = 0;
    check_error(mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &flag));
}

void Player::pause() {
    int flag = 1;
    check_error(mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &flag));
}

void Player::toggle_pause() {
    int flag = 0;
    if (mpv_get_property(mpv, "pause", MPV_FORMAT_FLAG, &flag) >= 0) {
        flag = !flag;
        check_error(mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &flag));
    }
}

void Player::stop() {
    const char* cmd[] = {"stop", nullptr};
    check_error(mpv_command(mpv, cmd));
}

bool Player::is_playing() {
    int flag = 0;
    if (mpv_get_property(mpv, "pause", MPV_FORMAT_FLAG, &flag) < 0) return false;
    return !flag;
}

bool Player::is_paused() {
    int flag = 0;
    if (mpv_get_property(mpv, "pause", MPV_FORMAT_FLAG, &flag) < 0) return false;
    return flag;
}

bool Player::is_idle() {
    int flag = 1;
    if (mpv_get_property(mpv, "idle-active", MPV_FORMAT_FLAG, &flag) < 0) return true;
    return flag;
}

double Player::get_position() {
    double pos = 0.0;
    if (mpv_get_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &pos) < 0) return 0.0;
    return pos;
}

double Player::get_duration() {
    double dur = 0.0;
    if (mpv_get_property(mpv, "duration", MPV_FORMAT_DOUBLE, &dur) < 0) return 0.0;
    return dur;
}

int Player::get_volume() {
    double vol = 100.0;
    if (mpv_get_property(mpv, "volume", MPV_FORMAT_DOUBLE, &vol) < 0) return 100;
    return static_cast<int>(vol);
}

void Player::set_volume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 150) volume = 150;
    double vol = static_cast<double>(volume);
    check_error(mpv_set_property(mpv, "volume", MPV_FORMAT_DOUBLE, &vol));
}

void Player::seek(double seconds) {
    std::string seconds_str = std::to_string(seconds);
    const char* cmd[] = {"seek", seconds_str.c_str(), "relative", nullptr};
    check_error(mpv_command(mpv, cmd));
}

std::string Player::get_metadata(const std::string& key) {
    char* value = mpv_get_property_string(mpv, key.c_str());
    if (value) {
        std::string result = value;
        mpv_free(value);
        return result;
    }
    return "";
}

void Player::set_property(const std::string& name, const std::string& value) {
    check_error(mpv_set_property_string(mpv, name.c_str(), value.c_str()));
}

static inline float db_to_linear(float db) {
    if (db <= -55.0f) return 0.0f;
    if (db >= 0.0f) return 1.0f;
    float norm = (db + 50.0f) / 50.0f;
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    return norm;
}

AudioLevelStats Player::get_audio_stats() {
    AudioLevelStats stats;
    if (!mpv) return stats;

    mpv_node node;
    if (mpv_get_property(mpv, "af-metadata/astats", MPV_FORMAT_NODE, &node) >= 0) {
        if (node.format == MPV_FORMAT_NODE_MAP && node.u.list) {
            stats.valid = true;
            for (int i = 0; i < node.u.list->num; ++i) {
                const char* key = node.u.list->keys[i];
                if (node.u.list->values[i].format != MPV_FORMAT_STRING) continue;
                const char* val_str = node.u.list->values[i].u.string;
                if (!val_str) continue;

                if (strcmp(key, "lavfi.astats.Overall.RMS_level") == 0) {
                    stats.rms_overall = db_to_linear(safe_stof(val_str, -60.0f));
                } else if (strcmp(key, "lavfi.astats.Overall.Peak_level") == 0) {
                    stats.peak_overall = db_to_linear(safe_stof(val_str, -60.0f));
                } else if (strcmp(key, "lavfi.astats.1.RMS_level") == 0) {
                    stats.rms_left = db_to_linear(safe_stof(val_str, -60.0f));
                } else if (strcmp(key, "lavfi.astats.2.RMS_level") == 0) {
                    stats.rms_right = db_to_linear(safe_stof(val_str, -60.0f));
                } else if (strstr(key, "Zero_crossings_rate") != nullptr) {
                    stats.zero_crossings = safe_stof(val_str, 0.05f);
                }
            }
            if (stats.rms_right <= 0.001f) {
                stats.rms_right = (stats.rms_left > 0.001f) ? stats.rms_left : stats.rms_overall;
            }
        }
        mpv_free_node_contents(&node);
    }
    return stats;
}
