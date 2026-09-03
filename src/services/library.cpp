#include "library.hpp"
#include "utils.hpp"
#include <algorithm>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

Library::Library() {
    root_path = get_home_music_dir();
}

void Library::set_root(const std::string& path) {
    if (fs::exists(path)) {
        root_path = path;
    }
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

bool Library::is_audio_file(const std::string& filename) {
    fs::path p(filename);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == ".mp3"  || ext == ".wav"  || ext == ".flac" ||
            ext == ".m4a"  || ext == ".ogg"  || ext == ".opus" ||
            ext == ".aac"  || ext == ".webm" || ext == ".wma"  ||
            ext == ".aiff" || ext == ".alac");
}

std::vector<LibraryItem> Library::list_directory(const std::string& path) {
    std::vector<LibraryItem> items;
    std::error_code ec;

    if (!fs::exists(path, ec)) {
        return items;
    }

    std::vector<LibraryItem> audio_files;

    try {
        for (const auto& entry : fs::directory_iterator(path, fs::directory_options::skip_permission_denied, ec)) {
            if (ec) continue;

            LibraryItem item;
            item.path = entry.path().string();
            item.name = entry.path().filename().string();
            item.is_directory = entry.is_directory(ec);

            if (item.is_directory) {
                // Ignore hidden directories
                if (!item.name.empty() && item.name.front() == '.') continue;
                items.push_back(item);
            } else if (is_audio_file(item.name)) {
                // Check if duration is cached in-memory
                auto it = duration_cache.find(item.path);
                if (it != duration_cache.end()) {
                    item.duration = it->second;
                }
                audio_files.push_back(item);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error listing directory: " << e.what() << std::endl;
    }

    // Sort directories alphabetically
    std::sort(items.begin(), items.end(), [](const LibraryItem& a, const LibraryItem& b) {
        return a.name < b.name;
    });

    // Sort audio files alphabetically
    std::sort(audio_files.begin(), audio_files.end(), [](const LibraryItem& a, const LibraryItem& b) {
        return a.name < b.name;
    });

    // If small number of audio files, resolve missing durations without noticeable lag
    if (audio_files.size() <= 20) {
        for (auto& file : audio_files) {
            if (file.duration.empty()) {
                file.duration = get_audio_duration(file.path);
                if (!file.duration.empty()) {
                    duration_cache[file.path] = file.duration;
                }
            }
        }
    }

    // Append audio files after directories
    items.insert(items.end(), audio_files.begin(), audio_files.end());
    return items;
}

std::vector<LibraryItem> Library::search(const std::string& query) {
    std::vector<LibraryItem> results;
    if (query.empty()) return results;

    std::string query_lower = query;
    std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);

    std::error_code ec;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(root_path, fs::directory_options::skip_permission_denied, ec)) {
            if (ec) continue;

            if (!entry.is_directory(ec) && is_audio_file(entry.path().filename().string())) {
                std::string filename = entry.path().filename().string();
                std::string filename_lower = filename;
                std::transform(filename_lower.begin(), filename_lower.end(), filename_lower.begin(), ::tolower);

                if (fuzzy_match(query_lower, filename_lower)) {
                    LibraryItem item;
                    item.path = entry.path().string();
                    item.name = filename;
                    item.is_directory = false;

                    auto it = duration_cache.find(item.path);
                    if (it != duration_cache.end()) {
                        item.duration = it->second;
                    }
                    results.push_back(item);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error searching library: " << e.what() << std::endl;
    }
    return results;
}

bool Library::fuzzy_match(const std::string& pattern, const std::string& text) {
    if (pattern.empty()) return true;
    size_t i = 0, j = 0;
    while (i < pattern.length() && j < text.length()) {
        if (pattern[i] == text[j]) {
            i++;
        }
        j++;
    }
    return i == pattern.length();
}
