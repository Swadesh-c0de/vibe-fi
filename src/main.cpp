#include "player.hpp"
#include "ui.hpp"
#include "utils.hpp"
#include "search.hpp"
#include "updater.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <clocale>

namespace fs = std::filesystem;

#ifndef VIBE_FI_VERSION
#define VIBE_FI_VERSION "1.1.1"
#endif

static void print_help(const char* prog_name) {
    std::cout << "Vibe-Fi - Terminal Music Player for Developers (v" << VIBE_FI_VERSION << ")\n\n"
              << "Usage:\n"
              << "  " << prog_name << "                      Launch interactive player\n"
              << "  " << prog_name << " <url>                Stream YouTube audio directly\n"
              << "  " << prog_name << " <file>               Play local audio file\n"
              << "  " << prog_name << " <search query>       Search and play track from YouTube\n"
              << "  " << prog_name << " --update | -u        Check and apply latest updates\n"
              << "  " << prog_name << " --no-update          Skip startup update check\n"
              << "  " << prog_name << " --help | -h          Show this help message\n"
              << "  " << prog_name << " --version | -v       Show version information\n\n"
              << "Controls:\n"
              << "  SPACE       Play / Pause toggle\n"
              << "  Left/Right  Seek backward / forward 5s\n"
              << "  +/-         Volume adjustment\n"
              << "  S           Search YouTube\n"
              << "  L           Browse Local Music Library\n"
              << "  P           Manage Playlists\n"
              << "  C           Interactive Play Queue\n"
              << "  T           Cycle Themes (Midnight, Matrix, Nord)\n"
              << "  V           Cycle Visualizers (Neon Flame, Stereo Bars, Pulse)\n"
              << "  ESC / Q     Back / Quit\n";
}

int main(int argc, char* argv[]) {
    std::setlocale(LC_ALL, "");
    std::setlocale(LC_NUMERIC, "C"); // libmpv requires LC_NUMERIC to remain "C"
    std::vector<std::string> startup_errors;
    std::vector<SearchResult> initial_queue;

    // Check for help, version, or update flags
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_help(argv[0]);
            return 0;
        }
        if (arg == "--version" || arg == "-v") {
            std::cout << "Vibe-Fi version " << VIBE_FI_VERSION << " (C++17, libmpv, ncurses)\n";
            return 0;
        }
        if (arg == "--update" || arg == "-u") {
            prompt_and_handle_update(argc, argv, true);
            return 0;
        }
    }

    // Check for --no-update flag and separate playable inputs
    bool skip_update = false;
    std::vector<std::string> playback_inputs;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--no-update") {
            skip_update = true;
        } else {
            playback_inputs.push_back(arg);
        }
    }

    // Automatic update check on startup
    if (!skip_update) {
        prompt_and_handle_update(argc, argv, false);
    }

    try {
        Player player;
        bool start_playback = false;

        for (const auto& input : playback_inputs) {
            std::string url_to_play = input;

            if (is_url(input)) {
                if (!is_online()) {
                    std::cerr << ":: Error: Internet connection required to stream online URL.\n";
                    startup_errors.push_back("Internet connection required to stream URL.");
                    continue;
                }
                std::cout << "Resolving stream URL: " << input << "..." << std::endl;
                try {
                    url_to_play = get_youtube_stream_url(input);
                } catch (const std::exception& e) {
                    startup_errors.push_back("Failed to resolve URL: " + input);
                    continue;
                }
            } else if (fs::exists(input)) {
                // Local file exists
                url_to_play = fs::absolute(input).string();
            } else {
                // Treat non-file input as YouTube search query
                if (!is_online()) {
                    std::cerr << ":: Error: Internet connection required to search YouTube.\n";
                    startup_errors.push_back("Internet connection required to search YouTube.");
                    continue;
                }
                std::cout << "Searching YouTube for: " << input << "..." << std::endl;
                auto search_hits = search_youtube(input, 5);
                if (!search_hits.empty()) {
                    url_to_play = search_hits.front().url;
                    initial_queue = search_hits;
                } else {
                    startup_errors.push_back("No results for: " + input);
                    continue;
                }
            }

            try {
                if (!start_playback) {
                    player.load(url_to_play, "replace");
                    start_playback = true;
                } else {
                    player.load(url_to_play, "append-play");
                }
            } catch (const std::exception& e) {
                startup_errors.push_back("Load error: " + std::string(e.what()));
            }
        }

        if (start_playback) {
            player.play();
        }

        UI ui(player);

        if (!start_playback && playback_inputs.empty()) {
            ui.set_mode(AppMode::INTRO);
        } else if (start_playback) {
            ui.set_mode(AppMode::PLAYBACK);
            if (!initial_queue.empty()) {
                ui.set_initial_queue(initial_queue);
            }
        }

        for (const auto& err : startup_errors) {
            ui.show_message(err);
        }

        ui.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
