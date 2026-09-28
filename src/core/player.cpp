#include "player.hpp"
#include "utils.hpp"
#include <stdexcept>
#include <iostream>

#include <clocale>
#include <cstring>

#if defined(__linux__)
#include <malloc.h>
#endif

Player::Player() : mpv(nullptr) {
    std::setlocale(LC_NUMERIC, "C"); // libmpv requires LC_NUMERIC="C" to parse decimal points reliably
    mpv = mpv_create();
    if (!mpv) {
        throw std::runtime_error("Failed to create libmpv context");
    }

    // Configure mpv defaults for optimal, resilient audio streaming
    check_error(mpv_set_option_string(mpv, "vo", "null"));                   // Audio only, disable video window
    check_error(mpv_set_option_string(mpv, "audio-display", "no"));          // Don't render embedded album art as video
    check_error(mpv_set_option_string(mpv, "ytdl", "yes"));                  // Enable YouTube extraction
    check_error(mpv_set_option_string(mpv, "ytdl-format", "251/140/bestaudio[ext=m4a]/bestaudio[ext=webm]/bestaudio/best"));

    // Optional optimizations: disable unused GUI scripts/overlays if supported by local libmpv version
    mpv_set_option_string(mpv, "video", "no");
    mpv_set_option_string(mpv, "osc", "no");
    mpv_set_option_string(mpv, "load-stats-overlay", "no");
    mpv_set_option_string(mpv, "load-console", "no");
    mpv_set_option_string(mpv, "load-osd-console", "no");
    mpv_set_option_string(mpv, "load-context-menu", "no");
    mpv_set_option_string(mpv, "load-positioning", "no");
    mpv_set_option_string(mpv, "load-select", "no");
    mpv_set_option_string(mpv, "load-commands", "no");
    mpv_set_option_string(mpv, "load-auto-profiles", "no");

    // Audio output fallback chain (PipeWire -> PulseAudio -> ALSA -> system default)
    mpv_set_option_string(mpv, "ao", "pipewire,pulse,alsa,coreaudio,audiotrack,");

    // Network resilience: buffer audio stream safely while keeping memory under 35 MB
    mpv_set_option_string(mpv, "stream-lavf-o", "reconnect=1,reconnect_delay_max=5");
    mpv_set_option_string(mpv, "network-timeout", "30");
    mpv_set_option_string(mpv, "demuxer-max-bytes", "4MiB");
    mpv_set_option_string(mpv, "demuxer-max-back-bytes", "512KiB");
    mpv_set_option_string(mpv, "demuxer-readahead-secs", "15");

    // Request error/warning logs from mpv to record into vibe.log
    mpv_request_log_messages(mpv, "warn");

    // Dynamically locate yt-dlp across PATH and common install directories
    std::string ytdl_path = find_executable("yt-dlp");
    if (!ytdl_path.empty()) {
        std::string script_opt = "ytdl_hook-ytdl_path=" + ytdl_path;
        mpv_set_option_string(mpv, "script-opts", script_opt.c_str());
    }

    // Attach real-time audio statistics filter for cross-platform beat & loudness analysis
    mpv_set_option_string(mpv, "af", "@astats:lavfi=[astats=metadata=1:reset=1:length=0.04]");

    check_error(mpv_initialize(mpv));
#if defined(__linux__)
    malloc_trim(0);
#endif
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
    clear_playback_flags();
    loading_active = true;
    const char* cmd[] = {"loadfile", path.c_str(), mode.c_str(), nullptr};
    check_error(mpv_command(mpv, cmd));
}

void Player::play() {
    if (!mpv) return;
    int flag = 0;
    int ret = mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &flag);
    if (ret < 0) last_error_str = mpv_error_string(ret);
}

void Player::pause() {
    if (!mpv) return;
    int flag = 1;
    int ret = mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &flag);
    if (ret < 0) last_error_str = mpv_error_string(ret);
}

void Player::toggle_pause() {
    if (!mpv) return;
    int flag = 0;
    if (mpv_get_property(mpv, "pause", MPV_FORMAT_FLAG, &flag) >= 0) {
        flag = !flag;
        int ret = mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &flag);
        if (ret < 0) last_error_str = mpv_error_string(ret);
    }
}

void Player::stop() {
    clear_playback_flags();
    if (!mpv) return;
    const char* cmd[] = {"stop", nullptr};
    int ret = mpv_command(mpv, cmd);
    if (ret < 0) last_error_str = mpv_error_string(ret);
}

bool Player::is_buffering() {
    if (!mpv) return false;
    int flag = 0;
    if (mpv_get_property(mpv, "paused-for-cache", MPV_FORMAT_FLAG, &flag) >= 0 && flag) {
        return true;
    }
    return false;
}

bool Player::is_loading() {
    if (!mpv) return false;
    if (loading_active) return true;
    if (!is_idle() && !is_paused()) {
        double pos = 0.0;
        if (mpv_get_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &pos) < 0) {
            return true;
        }
    }
    return false;
}

bool Player::is_idle() {
    if (!mpv) return true;
    int flag = 1;
    if (mpv_get_property(mpv, "idle-active", MPV_FORMAT_FLAG, &flag) < 0) return true;
    return flag != 0;
}

bool Player::is_paused() {
    if (!mpv) return false;
    if (is_idle()) return false;
    int flag = 0;
    if (mpv_get_property(mpv, "pause", MPV_FORMAT_FLAG, &flag) < 0) return false;
    return flag != 0;
}

bool Player::is_playing() {
    if (!mpv) return false;
    if (is_idle() || is_paused() || is_loading() || is_buffering()) return false;
    double pos = 0.0;
    if (mpv_get_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &pos) < 0) return false;
    return true;
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
    if (!mpv) return;
    if (volume < 0) volume = 0;
    if (volume > 150) volume = 150;
    double vol = static_cast<double>(volume);
    int ret = mpv_set_property(mpv, "volume", MPV_FORMAT_DOUBLE, &vol);
    if (ret < 0) last_error_str = mpv_error_string(ret);
}

void Player::seek(double seconds) {
    if (!mpv) return;
    std::string seconds_str = std::to_string(seconds);
    const char* cmd[] = {"seek", seconds_str.c_str(), "relative", nullptr};
    int ret = mpv_command(mpv, cmd);
    if (ret < 0) last_error_str = mpv_error_string(ret);
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

void Player::poll_events() {
    if (!mpv) return;
    while (true) {
        mpv_event* event = mpv_wait_event(mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) break;

        switch (event->event_id) {
            case MPV_EVENT_START_FILE:
                loading_active = true;
                track_finished = false;
                playback_error = false;
                last_error_str.clear();
                break;
            case MPV_EVENT_FILE_LOADED:
                loading_active = false;
                break;
            case MPV_EVENT_LOG_MESSAGE: {
                mpv_event_log_message* msg = static_cast<mpv_event_log_message*>(event->data);
                if (msg && msg->log_level <= MPV_LOG_LEVEL_WARN) {
                    std::fprintf(stderr, "[mpv::%s] %s\n", msg->prefix, msg->text);
                    std::fflush(stderr);
                }
                break;
            }
            case MPV_EVENT_END_FILE: {
                loading_active = false;
                mpv_event_end_file* eef = static_cast<mpv_event_end_file*>(event->data);
                if (eef) {
                    std::fprintf(stderr, "[vibe::mpv] END_FILE reason=%d (%s) error=%d (%s)\n",
                                 eef->reason,
                                 (eef->reason == MPV_END_FILE_REASON_EOF ? "EOF" :
                                  eef->reason == MPV_END_FILE_REASON_STOP ? "STOP" :
                                  eef->reason == MPV_END_FILE_REASON_QUIT ? "QUIT" :
                                  eef->reason == MPV_END_FILE_REASON_ERROR ? "ERROR" :
                                  eef->reason == MPV_END_FILE_REASON_REDIRECT ? "REDIRECT" : "OTHER"),
                                 eef->error, mpv_error_string(eef->error));
                    std::fflush(stderr);

                    if (eef->reason == MPV_END_FILE_REASON_EOF) {
                        track_finished = true;
                    } else if (eef->reason == MPV_END_FILE_REASON_ERROR) {
                        playback_error = true;
                        last_error_str = mpv_error_string(eef->error);
                    }
                }
                break;
            }
            default:
                break;
        }
    }
}

bool Player::consume_track_finished() {
    if (track_finished) {
        track_finished = false;
        return true;
    }
    return false;
}

bool Player::consume_playback_error() {
    if (playback_error) {
        playback_error = false;
        return true;
    }
    return false;
}

void Player::clear_playback_flags() {
    track_finished = false;
    playback_error = false;
    loading_active = false;
    last_error_str.clear();
}
