#ifndef MPRIS_HPP
#define MPRIS_HPP

#include <string>
#include <cstdint>

enum class MprisAction {
    NONE,
    PLAY_PAUSE,
    PLAY,
    PAUSE,
    NEXT,
    PREVIOUS,
    STOP
};

struct MprisMetadata {
    std::string title;
    std::string artist;
    std::string album;
    std::string url;
    int64_t length_us = 0;
    std::string playback_status = "Stopped";
    double volume = 1.0;
    int64_t position_us = 0;
};

// Starts the MPRIS background server thread (if enabled and supported)
void start_mpris_server();

// Signals the MPRIS server thread to shut down cleanly
void stop_mpris_server();

// Polls for pending MPRIS commands received on the background thread
// Must be called from the main UI thread to ensure thread-safe UI updates
bool poll_mpris_action(MprisAction& action);

// Updates active track metadata and playback status exposed over D-Bus
void update_mpris_metadata(const MprisMetadata& meta);
void update_mpris_playback_status(const std::string& status);
void update_mpris_position(int64_t position_us);

#endif // MPRIS_HPP
