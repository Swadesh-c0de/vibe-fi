#ifndef MPRIS_HPP
#define MPRIS_HPP

enum class MprisAction {
    NONE,
    PLAY_PAUSE,
    NEXT,
    PREVIOUS,
    STOP
};

// Starts the MPRIS background server thread (if enabled and supported)
void start_mpris_server();

// Signals the MPRIS server thread to shut down cleanly
void stop_mpris_server();

// Polls for pending MPRIS commands received on the background thread
// Must be called from the main UI thread to ensure thread-safe UI updates
bool poll_mpris_action(MprisAction& action);

#endif // MPRIS_HPP
