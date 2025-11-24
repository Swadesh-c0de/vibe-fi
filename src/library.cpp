#include "library.hpp"
#include "utils.hpp"
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

Library::Library() {
    root_path = get_home_music_dir();
}

void Library::set_root(const std::string& path) {
    root_path = path;
}

std::string Library::get_home_music_dir() {
    const char* home = getenv("HOME");
    if (home) {
        std::string music_dir = std::string(home) + "/Music";
        if (fs::exists(music_dir)) return music_dir;
        return std::string(home);
    }
    return ".";
}

std::vector<LibraryItem> Library::list_directory(const std::string& path) {
    std::vector<LibraryItem> items;
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            LibraryItem item;
            item.path = entry.path().string();
            item.name = entry.path().filename().string();
            item.is_directory = entry.is_directory();
            
            // Filter for audio files or directories
            if (item.is_directory) {
                items.push_back(item);
            } else {
                std::string ext = entry.path().extension().string();
                // Simple check for common audio extensions
                if (ext == ".mp3" || ext == ".wav" || ext == ".flac" || ext == ".m4a" || ext == ".ogg") {
                    item.duration = get_audio_duration(item.path);
                    items.push_back(item);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error listing directory: " << e.what() << std::endl;
    }
    
    // Sort directories first, then files
    std::sort(items.begin(), items.end(), [](const LibraryItem& a, const LibraryItem& b) {
        if (a.is_directory != b.is_directory) return a.is_directory > b.is_directory;
        return a.name < b.name;
    });
    
    return items;
}

std::vector<LibraryItem> Library::search(const std::string& query) {
    std::vector<LibraryItem> results;
    // Recursive search - might be slow for large libraries
    try {
        for (const auto& entry : fs::recursive_directory_iterator(root_path)) {
            if (!entry.is_directory()) {
                std::string filename = entry.path().filename().string();
                // Case insensitive search
                std::string filename_lower = filename;
                std::string query_lower = query;
                std::transform(filename_lower.begin(), filename_lower.end(), filename_lower.begin(), ::tolower);
                std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);
                
                if (filename_lower.find(query_lower) != std::string::npos) {
                    std::string ext = entry.path().extension().string();
                    if (ext == ".mp3" || ext == ".wav" || ext == ".flac" || ext == ".m4a" || ext == ".ogg") {
                        LibraryItem item;
                        item.path = entry.path().string();
                        item.name = filename;
                        item.is_directory = false;
                        results.push_back(item);
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error searching library: " << e.what() << std::endl;
    }
    return results;
}
