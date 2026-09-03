#include "visualizer.hpp"
#include <cmath>
#include <algorithm>
#include <cstdlib>

Visualizer::Visualizer() {
    reset();
}

void Visualizer::reset() {
    flame_bars.clear();
    flame_peaks.clear();
    flame_hold.clear();
    flame_fall.clear();

    stereo_bars.clear();
    stereo_peaks.clear();
    stereo_hold.clear();
    stereo_fall.clear();

    pulse_bars.clear();
    pulse_peaks.clear();
    pulse_hold.clear();
    pulse_fall.clear();
}

void Visualizer::update_track_visual_profile(Player& player) {
    std::string title = player.get_metadata("media-title");
    if (title.empty()) title = player.get_metadata("filename");
    if (title.empty()) title = "default";

    if (current_profile.track_id == title) return;

    current_profile.track_id = title;

    // Deterministic hash of track name to give every song a unique, consistent musical groove
    size_t hash = 5381;
    for (char c : title) {
        hash = ((hash << 5) + hash) + static_cast<unsigned char>(c);
    }

    std::string lower_title = title;
    for (auto& c : lower_title) c = tolower(c);

    // Genre-aware & acoustic modulation
    if (lower_title.find("lofi") != std::string::npos || lower_title.find("chill") != std::string::npos ||
        lower_title.find("slow") != std::string::npos || lower_title.find("ambient") != std::string::npos ||
        lower_title.find("sleep") != std::string::npos) {
        current_profile.bpm = 74.0f + static_cast<float>(hash % 16); // 74-90 BPM (relaxed)
        current_profile.bass_weight = 1.35f;
        current_profile.mid_weight = 0.90f;
        current_profile.treble_weight = 0.70f;
    } else if (lower_title.find("remix") != std::string::npos || lower_title.find("club") != std::string::npos ||
               lower_title.find("dance") != std::string::npos || lower_title.find("edm") != std::string::npos ||
               lower_title.find("house") != std::string::npos || lower_title.find("bass") != std::string::npos) {
        current_profile.bpm = 124.0f + static_cast<float>(hash % 18); // 124-142 BPM (dance/EDM)
        current_profile.bass_weight = 1.45f;
        current_profile.mid_weight = 1.05f;
        current_profile.treble_weight = 1.30f;
    } else if (lower_title.find("rock") != std::string::npos || lower_title.find("metal") != std::string::npos ||
               lower_title.find("punk") != std::string::npos || lower_title.find("guitar") != std::string::npos) {
        current_profile.bpm = 132.0f + static_cast<float>(hash % 32); // 132-164 BPM (energetic)
        current_profile.bass_weight = 1.10f;
        current_profile.mid_weight = 1.40f;
        current_profile.treble_weight = 1.20f;
    } else if (lower_title.find("rap") != std::string::npos || lower_title.find("hip hop") != std::string::npos ||
               lower_title.find("trap") != std::string::npos || lower_title.find("drill") != std::string::npos) {
        current_profile.bpm = 88.0f + static_cast<float>(hash % 24); // 88-112 BPM (heavy boom-bap / trap)
        current_profile.bass_weight = 1.55f;
        current_profile.mid_weight = 1.00f;
        current_profile.treble_weight = 1.10f;
    } else {
        // General tracks: dynamic variety based on track name
        current_profile.bpm = 85.0f + static_cast<float>(hash % 60); // 85-145 BPM
        current_profile.bass_weight = 0.85f + ((hash >> 4) % 50) * 0.01f;
        current_profile.mid_weight = 0.85f + ((hash >> 8) % 40) * 0.01f;
        current_profile.treble_weight = 0.80f + ((hash >> 12) % 45) * 0.01f;
    }

    current_profile.rhythm_swing = ((hash >> 16) % 25) * 0.01f; // 0.00 to 0.25 swing
    current_profile.energy_variance = 0.88f + ((hash >> 20) % 24) * 0.01f;
}

static const char* BLOCK_CHARS[9] = {
    " ",
    "\xe2\x96\x81", //   (lower 1/8)
    "\xe2\x96\x82", // ▂ (lower 2/8)
    "\xe2\x96\x83", // ▃ (lower 3/8)
    "\xe2\x96\x84", // ▄ (lower 4/8)
    "\xe2\x96\x85", // ▅ (lower 5/8)
    "\xe2\x96\x86", // ▆ (lower 6/8)
    "\xe2\x96\x87", // ▇ (lower 7/8)
    "\xe2\x96\x88"  // █ (full block)
};
static const char* PEAK_CHAR = "\xe2\x96\x94"; // ▔ (upper 1/8 block)

void Visualizer::render(WINDOW* win, Player& player, VisualizerMode mode) {
    werase(win);
    update_track_visual_profile(player);

    bool is_active = player.is_playing() && !player.is_paused() && !player.is_idle();
    AudioLevelStats stats = player.get_audio_stats();
    double pos = player.get_position();
    float track_bps = current_profile.bpm / 60.0f;

    std::string mode_title = "VISUALIZER: NEON FLAME";
    if (mode == VisualizerMode::STEREO_BARS) {
        mode_title = "VISUALIZER: STEREO BARS";
    } else if (mode == VisualizerMode::PULSE) {
        mode_title = "VISUALIZER: PULSE";
    }

    if (is_active) {
        // Live rhythmic 4-beat metronome indicator
        int beat_step = (track_bps > 0.05f) ? (static_cast<int>(pos * track_bps) % 4) : 0;
        std::string metro = "[♫";
        for (int b = 0; b < 4; ++b) {
            metro += (b == beat_step) ? " ●" : " ○";
        }
        metro += " ]";

        if (stats.valid && stats.peak_overall > 0.001f) {
            int lvl_pct = static_cast<int>(stats.peak_overall * 100.0f);
            mode_title += " " + metro + " [" + std::to_string(lvl_pct) + "% PEAK]";
        } else {
            int bpm_display = static_cast<int>(current_profile.bpm);
            mode_title += " " + metro + " [~" + std::to_string(bpm_display) + " BPM]";
        }
    }

    // Draw borders with title
    box(win, 0, 0);
    if (!mode_title.empty()) {
        wattron(win, COLOR_PAIR(1) | A_BOLD);
        mvwprintw(win, 0, 2, " %s ", mode_title.c_str());
        wattroff(win, COLOR_PAIR(1) | A_BOLD);
    }

    int height, width;
    getmaxyx(win, height, width);

    int draw_h = height - 2;
    int draw_w = width - 2;
    if (draw_h <= 0 || draw_w <= 0) {
        wnoutrefresh(win);
        return;
    }

    float vol = std::clamp(player.get_volume() / 100.0f, 0.2f, 1.2f);

    switch (mode) {
        case VisualizerMode::NEON_FLAME:
            render_neon_flame(win, player, draw_h, draw_w, pos, vol, stats);
            break;
        case VisualizerMode::STEREO_BARS:
            render_stereo_bars(win, player, draw_h, draw_w, pos, vol, stats);
            break;
        case VisualizerMode::PULSE:
            render_pulse(win, player, draw_h, draw_w, pos, vol, stats);
            break;
    }

    wnoutrefresh(win);
}

void Visualizer::render_neon_flame(WINDOW* win, Player& player, int draw_h, int draw_w, double pos, float vol, const AudioLevelStats& stats) {
    bool is_active = player.is_playing() && !player.is_paused() && !player.is_idle();
    float max_sub_levels = draw_h * 8.0f;

    float live_rms = stats.valid ? stats.rms_overall : 0.35f;
    float live_peak = stats.valid ? stats.peak_overall : 0.45f;
    float live_left = stats.valid ? stats.rms_left : live_rms;
    float live_right = stats.valid ? stats.rms_right : live_rms;
    float live_pitch = stats.valid ? std::clamp(stats.zero_crossings * 12.0f, 0.20f, 1.5f) : 1.0f;

    float attack_speed = 0.90f;
    float decay_speed  = 0.26f;

    float track_bps = current_profile.bpm / 60.0f;
    float beat_interval = 1.0f / std::max(0.1f, track_bps);

    int bar_w = (draw_w >= 80) ? 2 : 1;
    int gap = 1;
    int slot_w = bar_w + gap;
    int num_bars = draw_w / slot_w;
    if (num_bars < 6) num_bars = 6;
    int total_bars_w = num_bars * slot_w - gap;
    int start_x = 1 + std::max(0, (draw_w - total_bars_w) / 2);

    if (static_cast<int>(flame_bars.size()) != num_bars) {
        flame_bars.assign(num_bars, 0.0f);
        flame_peaks.assign(num_bars, 0.0f);
        flame_hold.assign(num_bars, 0);
        flame_fall.assign(num_bars, 0.0f);
    }

    int center_idx = num_bars / 2;

    if (is_active) {
        float kick_phase = fmod(pos, beat_interval) / beat_interval;
        float kick = exp(-kick_phase * (4.8f + current_profile.rhythm_swing * 4.0f)) * current_profile.bass_weight;

        float snare_phase = fmod(pos + beat_interval * 0.5f, beat_interval) / beat_interval;
        float snare = exp(-snare_phase * 7.5f) * current_profile.mid_weight;

        float hihat_phase = fmod(pos, beat_interval * 0.25f) / (beat_interval * 0.25f);
        float hihat = exp(-hihat_phase * 11.0f) * current_profile.treble_weight;

        for (int i = 0; i < num_bars; ++i) {
            float dist = fabsf(static_cast<float>(i - center_idx)) / std::max(1, center_idx);
            float channel_energy = (i < center_idx) ? live_left : live_right;
            if (!stats.valid) channel_energy = live_rms;

            float center_punch = kick * std::max(0.0f, 1.0f - dist * 2.6f) * (0.45f + live_peak * 0.85f);
            float mid_dance = snare * std::max(0.0f, 1.0f - fabsf(dist - 0.45f) * 3.0f) * (0.35f + channel_energy * 0.85f);
            float treble_flicker = (hihat * 0.40f + ((rand() % 16) * 0.01f)) * std::max(0.0f, dist - 0.35f) * 2.2f;

            float w1 = sin(pos * (track_bps * 3.8f * live_pitch) + (i * 0.42f)) * 0.22f;
            float w2 = cos(pos * (track_bps * 7.2f * live_pitch) - (dist * 4.8f)) * 0.16f;
            float w3 = sin(pos * (track_bps * 1.5f) + (dist * 3.14f)) * 0.12f;
            float flame_flutter = w1 + w2 + w3;

            float base_tilt = (1.05f - dist * 0.38f) * (0.36f + flame_flutter);
            float raw_energy = (base_tilt * (0.30f + channel_energy * 0.70f)
                                + center_punch * 0.85f 
                                + mid_dance * 0.65f 
                                + treble_flicker) * vol;

            if (stats.valid) {
                raw_energy *= (0.40f + live_peak * 0.80f);
            }

            if (raw_energy < 0.04f) raw_energy = 0.04f;
            if (raw_energy > 1.0f) raw_energy = 1.0f;

            float target = raw_energy * max_sub_levels;
            if (target > flame_bars[i]) {
                flame_bars[i] += (target - flame_bars[i]) * attack_speed;
            } else {
                flame_bars[i] -= (flame_bars[i] - target) * decay_speed;
            }

            if (flame_bars[i] >= flame_peaks[i]) {
                flame_peaks[i] = flame_bars[i];
                flame_hold[i] = 3;
                flame_fall[i] = 0.0f;
            } else {
                if (flame_hold[i] > 0) {
                    flame_hold[i]--;
                } else {
                    flame_fall[i] += 0.65f;
                    flame_peaks[i] -= flame_fall[i];
                    if (flame_peaks[i] < flame_bars[i]) flame_peaks[i] = flame_bars[i];
                }
            }
        }
    } else {
        for (int i = 0; i < num_bars; ++i) {
            flame_bars[i] *= 0.85f;
            flame_peaks[i] *= 0.85f;
            if (flame_bars[i] < 0.5f) flame_bars[i] = 0.0f;
            if (flame_peaks[i] < 0.5f) flame_peaks[i] = 0.0f;
        }
    }

    int height = draw_h + 2;
    for (int i = 0; i < num_bars; ++i) {
        int val = static_cast<int>(flame_bars[i]);
        int full_cells = val / 8;
        int rem = val % 8;
        int peak_cell = static_cast<int>(flame_peaks[i]) / 8;
        int bar_x = start_x + i * slot_w;

        for (int y = 0; y < draw_h; ++y) {
            int draw_y = height - 2 - y;
            int color_pair = 7;
            if (y >= draw_h * 2 / 3) {
                color_pair = 9; // High / Hot flame
            } else if (y >= draw_h / 3) {
                color_pair = 8; // Mid flame
            }

            if (y < full_cells) {
                wattron(win, COLOR_PAIR(color_pair) | A_BOLD);
                for (int k = 0; k < bar_w; ++k) {
                    mvwaddstr(win, draw_y, bar_x + k, "█");
                }
                wattroff(win, COLOR_PAIR(color_pair) | A_BOLD);
            } else if (y == full_cells && rem > 0) {
                wattron(win, COLOR_PAIR(color_pair) | A_BOLD);
                for (int k = 0; k < bar_w; ++k) {
                    mvwaddstr(win, draw_y, bar_x + k, BLOCK_CHARS[rem]);
                }
                wattroff(win, COLOR_PAIR(color_pair) | A_BOLD);
            } else if (y == peak_cell && peak_cell > full_cells && peak_cell < draw_h) {
                const char* crown_sym = (peak_cell >= draw_h * 2 / 3) ? "✦" : ((peak_cell >= draw_h / 3) ? "▲" : PEAK_CHAR);
                wattron(win, COLOR_PAIR(10) | A_BOLD);
                for (int k = 0; k < bar_w; ++k) {
                    mvwaddstr(win, draw_y, bar_x + k, crown_sym);
                }
                wattroff(win, COLOR_PAIR(10) | A_BOLD);
            }
        }
    }

    wattron(win, COLOR_PAIR(7) | A_DIM);
    mvwhline(win, height - 2, 1, ACS_HLINE, draw_w);
    wattroff(win, COLOR_PAIR(7) | A_DIM);
}

void Visualizer::render_stereo_bars(WINDOW* win, Player& player, int draw_h, int draw_w, double pos, float vol, const AudioLevelStats& stats) {
    bool is_active = player.is_playing() && !player.is_paused() && !player.is_idle();
    float max_sub_levels = draw_h * 8.0f;

    float live_rms = stats.valid ? stats.rms_overall : 0.35f;
    float live_peak = stats.valid ? stats.peak_overall : 0.45f;
    float live_left = stats.valid ? stats.rms_left : live_rms;
    float live_right = stats.valid ? stats.rms_right : live_rms;
    float live_pitch = stats.valid ? std::clamp(stats.zero_crossings * 12.0f, 0.15f, 1.6f) : 1.0f;

    float attack_speed = stats.valid ? (0.22f + live_peak * 0.55f) : 0.60f;
    float decay_speed  = stats.valid ? (0.07f + live_rms * 0.14f) : 0.18f;

    float track_bps = current_profile.bpm / 60.0f;
    float beat_interval = 1.0f / std::max(0.1f, track_bps);

    int bar_w = (draw_w >= 80) ? 2 : 1;
    int gap = 1;
    int slot_w = bar_w + gap;
    int num_bars = draw_w / slot_w;
    if (num_bars < 4) num_bars = 4;
    int total_bars_w = num_bars * slot_w - gap;
    int start_x = 1 + std::max(0, (draw_w - total_bars_w) / 2);

    if (static_cast<int>(stereo_bars.size()) != num_bars) {
        stereo_bars.assign(num_bars, 0.0f);
        stereo_peaks.assign(num_bars, 0.0f);
        stereo_hold.assign(num_bars, 0);
        stereo_fall.assign(num_bars, 0.0f);
    }

    if (is_active) {
        float kick_phase = fmod(pos, beat_interval) / beat_interval;
        float kick = exp(-kick_phase * (5.5f + current_profile.rhythm_swing * 4.0f)) * current_profile.bass_weight;

        float snare_phase = fmod(pos + beat_interval * 0.5f, beat_interval) / beat_interval;
        float snare = exp(-snare_phase * 8.0f) * current_profile.mid_weight;

        float hihat_phase = fmod(pos, beat_interval * 0.25f) / (beat_interval * 0.25f);
        float hihat = exp(-hihat_phase * 12.0f) * current_profile.treble_weight;

        int half_point = num_bars / 2;

        for (int i = 0; i < num_bars; ++i) {
            float norm = static_cast<float>(i) / std::max(1, num_bars - 1);
            float channel_energy = (i < half_point) ? live_left : live_right;
            if (!stats.valid) channel_energy = live_rms;

            float tilt = (0.95f * current_profile.bass_weight) - (norm * 0.38f * (2.0f - current_profile.treble_weight));

            float w1 = sin(pos * (track_bps * 3.2f * live_pitch) + i * 0.28f);
            float w2 = cos(pos * (track_bps * 5.8f * live_pitch) - i * 0.52f);
            float w3 = sin(pos * (track_bps * 1.4f) + i * 0.12f);
            float wave = (w1 * 0.42f + w2 * 0.36f + w3 * 0.22f);

            float bass_boost = kick * std::max(0.0f, 1.0f - norm * 2.8f) * (0.4f + live_peak * 0.7f);
            float mid_boost = snare * std::max(0.0f, 1.0f - fabsf(norm - 0.48f) * 3.0f) * (0.3f + live_peak * 0.6f);
            float treble_sparkle = (hihat * 0.30f + ((rand() % 10) * 0.01f)) * std::max(0.0f, norm - 0.50f) * 2.0f * (0.3f + live_peak * 0.7f);

            float raw_energy = (tilt * (0.28f + 0.44f * wave) * channel_energy * 1.6f
                                + bass_boost * 0.70f 
                                + mid_boost * 0.50f 
                                + treble_sparkle) * vol;

            if (stats.valid) {
                raw_energy *= (0.25f + live_peak * 0.85f);
            }

            if (raw_energy < 0.02f) raw_energy = 0.02f;
            if (raw_energy > 1.0f) raw_energy = 1.0f;

            float target = raw_energy * max_sub_levels;
            if (target > stereo_bars[i]) {
                stereo_bars[i] += (target - stereo_bars[i]) * attack_speed;
            } else {
                stereo_bars[i] -= (stereo_bars[i] - target) * decay_speed;
            }

            if (stereo_bars[i] >= stereo_peaks[i]) {
                stereo_peaks[i] = stereo_bars[i];
                stereo_hold[i] = (stats.valid && live_peak < 0.3f) ? 2 : 5;
                stereo_fall[i] = 0.0f;
            } else {
                if (stereo_hold[i] > 0) {
                    stereo_hold[i]--;
                } else {
                    stereo_fall[i] += (stats.valid && live_peak < 0.3f) ? 0.35f : 0.55f;
                    stereo_peaks[i] -= stereo_fall[i];
                    if (stereo_peaks[i] < stereo_bars[i]) stereo_peaks[i] = stereo_bars[i];
                }
            }
        }
    } else {
        for (int i = 0; i < num_bars; ++i) {
            stereo_bars[i] *= 0.85f;
            stereo_peaks[i] *= 0.85f;
            if (stereo_bars[i] < 0.5f) stereo_bars[i] = 0.0f;
            if (stereo_peaks[i] < 0.5f) stereo_peaks[i] = 0.0f;
        }
    }

    int height = draw_h + 2;
    for (int i = 0; i < num_bars; ++i) {
        int val = static_cast<int>(stereo_bars[i]);
        int full_cells = val / 8;
        int rem = val % 8;
        int peak_cell = static_cast<int>(stereo_peaks[i]) / 8;
        int bar_x = start_x + i * slot_w;

        for (int y = 0; y < draw_h; ++y) {
            int draw_y = height - 2 - y;
            int color_pair = 7;
            if (y >= draw_h * 2 / 3) {
                color_pair = 9;
            } else if (y >= draw_h / 3) {
                color_pair = 8;
            }

            if (y < full_cells) {
                wattron(win, COLOR_PAIR(color_pair) | A_BOLD);
                for (int k = 0; k < bar_w; ++k) {
                    mvwaddstr(win, draw_y, bar_x + k, "█");
                }
                wattroff(win, COLOR_PAIR(color_pair) | A_BOLD);
            } else if (y == full_cells && rem > 0) {
                wattron(win, COLOR_PAIR(color_pair) | A_BOLD);
                for (int k = 0; k < bar_w; ++k) {
                    mvwaddstr(win, draw_y, bar_x + k, BLOCK_CHARS[rem]);
                }
                wattroff(win, COLOR_PAIR(color_pair) | A_BOLD);
            } else if (y == peak_cell && peak_cell > full_cells && peak_cell < draw_h) {
                wattron(win, COLOR_PAIR(10) | A_BOLD);
                for (int k = 0; k < bar_w; ++k) {
                    mvwaddstr(win, draw_y, bar_x + k, PEAK_CHAR);
                }
                wattroff(win, COLOR_PAIR(10) | A_BOLD);
            }
        }
    }
}

void Visualizer::render_pulse(WINDOW* win, Player& player, int draw_h, int draw_w, double pos, float vol, const AudioLevelStats& stats) {
    bool is_active = player.is_playing() && !player.is_paused() && !player.is_idle();
    float max_sub_levels = draw_h * 8.0f;

    float live_rms = stats.valid ? stats.rms_overall : 0.35f;
    float live_peak = stats.valid ? stats.peak_overall : 0.45f;
    float attack_speed = 0.90f;
    float decay_speed  = 0.26f;

    float track_bps = current_profile.bpm / 60.0f;
    float beat_interval = 1.0f / std::max(0.1f, track_bps);

    int bar_w = (draw_w >= 80) ? 2 : 1;
    int gap = 1;
    int slot_w = bar_w + gap;
    int num_bars = draw_w / slot_w;
    if (num_bars < 4) num_bars = 4;
    int total_bars_w = num_bars * slot_w - gap;
    int start_x = 1 + std::max(0, (draw_w - total_bars_w) / 2);

    if (static_cast<int>(pulse_bars.size()) != num_bars) {
        pulse_bars.assign(num_bars, 0.0f);
        pulse_peaks.assign(num_bars, 0.0f);
        pulse_hold.assign(num_bars, 0);
        pulse_fall.assign(num_bars, 0.0f);
    }

    int center = num_bars / 2;
    if (is_active) {
        float kick_phase = fmod(pos, beat_interval) / beat_interval;
        float kick = exp(-kick_phase * 6.0f) * current_profile.bass_weight * (0.35f + live_peak * 0.75f);

        for (int i = 0; i < num_bars; ++i) {
            float dist_from_center = fabsf(static_cast<float>(i - center)) / std::max(1, center);
            float ripple = sin(pos * (track_bps * 4.5f) - dist_from_center * 8.0f) * 0.5f + 0.5f;
            float falloff = 1.0f - dist_from_center * 0.65f;

            float energy = (kick * 0.85f * (1.0f - dist_from_center * 0.8f) + ripple * 0.45f * falloff)
                           * vol * (0.3f + live_rms * 0.85f);
            if (energy < 0.02f) energy = 0.02f;
            if (energy > 1.0f) energy = 1.0f;

            float target = energy * max_sub_levels;
            if (target > pulse_bars[i]) {
                pulse_bars[i] += (target - pulse_bars[i]) * attack_speed;
            } else {
                pulse_bars[i] -= (pulse_bars[i] - target) * decay_speed;
            }

            if (pulse_bars[i] >= pulse_peaks[i]) {
                pulse_peaks[i] = pulse_bars[i];
                pulse_hold[i] = 4;
                pulse_fall[i] = 0.0f;
            } else {
                if (pulse_hold[i] > 0) {
                    pulse_hold[i]--;
                } else {
                    pulse_fall[i] += 0.5f;
                    pulse_peaks[i] -= pulse_fall[i];
                    if (pulse_peaks[i] < pulse_bars[i]) pulse_peaks[i] = pulse_bars[i];
                }
            }
        }
    } else {
        for (int i = 0; i < num_bars; ++i) {
            pulse_bars[i] *= 0.85f;
            pulse_peaks[i] *= 0.85f;
            if (pulse_bars[i] < 0.5f) pulse_bars[i] = 0.0f;
            if (pulse_peaks[i] < 0.5f) pulse_peaks[i] = 0.0f;
        }
    }

    int height = draw_h + 2;
    for (int i = 0; i < num_bars; ++i) {
        int val = static_cast<int>(pulse_bars[i]);
        int full_cells = val / 8;
        int rem = val % 8;
        int peak_cell = static_cast<int>(pulse_peaks[i]) / 8;
        int bar_x = start_x + i * slot_w;

        for (int y = 0; y < draw_h; ++y) {
            int draw_y = height - 2 - y;
            int color_pair = 7;
            if (y >= draw_h * 2 / 3) {
                color_pair = 9;
            } else if (y >= draw_h / 3) {
                color_pair = 8;
            }

            if (y < full_cells) {
                wattron(win, COLOR_PAIR(color_pair) | A_BOLD);
                for (int k = 0; k < bar_w; ++k) {
                    mvwaddstr(win, draw_y, bar_x + k, "█");
                }
                wattroff(win, COLOR_PAIR(color_pair) | A_BOLD);
            } else if (y == full_cells && rem > 0) {
                wattron(win, COLOR_PAIR(color_pair) | A_BOLD);
                for (int k = 0; k < bar_w; ++k) {
                    mvwaddstr(win, draw_y, bar_x + k, BLOCK_CHARS[rem]);
                }
                wattroff(win, COLOR_PAIR(color_pair) | A_BOLD);
            } else if (y == peak_cell && peak_cell > full_cells && peak_cell < draw_h) {
                wattron(win, COLOR_PAIR(10) | A_BOLD);
                for (int k = 0; k < bar_w; ++k) {
                    mvwaddstr(win, draw_y, bar_x + k, PEAK_CHAR);
                }
                wattroff(win, COLOR_PAIR(10) | A_BOLD);
            }
        }
    }
}
