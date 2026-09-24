#ifndef PLAYER_HPP
#define PLAYER_HPP

#include <string>
#include <mpv/client.h>

struct AudioLevelStats {
    float rms_overall = 0.0f;     // Normalized perceived loudness (0.0 to 1.0)
    float peak_overall = 0.0f;    // Normalized instantaneous peak hit (0.0 to 1.0)
    float rms_left = 0.0f;        // Left channel loudness (0.0 to 1.0)
    float rms_right = 0.0f;       // Right channel loudness (0.0 to 1.0)
    float zero_crossings = 0.05f; // Pitch / tone estimator (low = deep bass, high = bright/harsh)
    bool valid = false;
};

class Player {
public:
    Player();
    ~Player();

    // Player instances manage a raw mpv handle, non-copyable
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    void load(const std::string& path, const std::string& mode = "replace");
    void play();
    void pause();
    void toggle_pause();
    void stop();
    void seek(double seconds);
    
    bool is_playing();
    bool is_paused();
    bool is_idle();
    bool is_loading();
    bool is_buffering();
    double get_position();
    double get_duration();
    int get_volume();
    void set_volume(int volume);
    std::string get_metadata(const std::string& key);
    void set_property(const std::string& name, const std::string& value);
    AudioLevelStats get_audio_stats();

    // mpv event polling & lifecycle status
    void poll_events();
    bool has_track_finished() const { return track_finished; }
    bool consume_track_finished();
    bool has_playback_error() const { return playback_error; }
    bool consume_playback_error();
    std::string get_last_error() const { return last_error_str; }
    void clear_playback_flags();

private:
    mpv_handle* mpv;
    void check_error(int status);

    bool track_finished = false;
    bool playback_error = false;
    bool loading_active = false;
    std::string last_error_str;
};

#endif // PLAYER_HPP
