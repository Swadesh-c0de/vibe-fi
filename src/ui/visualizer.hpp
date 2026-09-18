#ifndef VISUALIZER_HPP
#define VISUALIZER_HPP

#include <ncurses.h>
#include <string>
#include <vector>
#include "player.hpp"

enum class VisualizerMode {
    CAVA_WAVE,    // Option 1: Cava Wave (Monstercat fluid smoothed spectrum with dynamic theme gradients) - DEFAULT
    NEON_FLAME,   // Option 2: Neon Flame (Dual-mirrored dynamic center volcano with dancing frequency columns & metronome)
    STEREO_BARS   // Option 3: Stereo Bars (8x sub-block graphic equalizer spectrum)
};

struct TrackVisualProfile {
    std::string track_id;
    float bpm = 120.0f;
    float bass_weight = 1.0f;
    float mid_weight = 1.0f;
    float treble_weight = 1.0f;
    float rhythm_swing = 0.0f;
    float energy_variance = 1.0f;
};

class Visualizer {
public:
    Visualizer();
    ~Visualizer() = default;

    // Reset internal animation states (e.g. on track change or window resize)
    void reset();

    // Render the active visualizer into the designated curses window
    void render(WINDOW* win, Player& player, VisualizerMode mode);

    // Profile retrieval
    const TrackVisualProfile& get_current_profile() const { return current_profile; }

private:
    TrackVisualProfile current_profile;

    // Internal state for CAVA_WAVE mode
    std::vector<float> cava_bars;
    std::vector<float> cava_peaks;
    std::vector<int> cava_hold;
    std::vector<float> cava_fall;

    // Internal state for NEON_FLAME mode
    std::vector<float> flame_bars;
    std::vector<float> flame_peaks;
    std::vector<int> flame_hold;
    std::vector<float> flame_fall;

    // Internal state for STEREO_BARS mode
    std::vector<float> stereo_bars;
    std::vector<float> stereo_peaks;
    std::vector<int> stereo_hold;
    std::vector<float> stereo_fall;

    void update_track_visual_profile(Player& player);
    void render_cava_wave(WINDOW* win, Player& player, int draw_h, int draw_w, double pos, float vol, const AudioLevelStats& stats);
    void render_neon_flame(WINDOW* win, Player& player, int draw_h, int draw_w, double pos, float vol, const AudioLevelStats& stats);
    void render_stereo_bars(WINDOW* win, Player& player, int draw_h, int draw_w, double pos, float vol, const AudioLevelStats& stats);
};

#endif // VISUALIZER_HPP
