#include "player.hpp"
#include "ui.hpp"
#include "utils.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    std::vector<std::string> startup_errors;
    try {
        Player player;
        bool start_playback = false;

        
        for (int i = 1; i < argc; ++i) {
            std::string input = argv[i];

            
            std::string url_to_play = input;
            if (is_url(input)) {
                std::cout << "Resolving URL: " << input << "..." << std::endl;
                try {
                    url_to_play = get_youtube_stream_url(input);
                } catch (const std::exception& e) {
                    std::cerr << "Error resolving URL " << input << ": " << e.what() << std::endl;
                    startup_errors.push_back("Failed: " + input);
                    continue; 
                }
            } else {
                if (!fs::exists(input)) {
                    // Treat as search query if not a file
                    std::cout << "Searching for: " << input << "..." << std::endl;
                    // We can't easily do "Search & Play" here without instantiating Search
                    // So let's just warn for now or assume it's a file error
                     std::cerr << "File not found: " << input << std::endl;
                     startup_errors.push_back("Not Found: " + input);
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
                startup_errors.push_back("Load Error: " + std::string(e.what()));
            }
        }
        
        if (start_playback) player.play(); 

        UI ui(player);
        
        if (!start_playback && argc == 1) {
            ui.set_mode(AppMode::INTRO);
        }
        
        for (const auto& err : startup_errors) {
            ui.show_message(err);
        }
        
        ui.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
