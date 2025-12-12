#include "playlist_manager.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <sstream>

namespace fs = std::filesystem;

PlaylistManager::PlaylistManager() {
    const char* home = getenv("HOME");
    if (home) {
        playlists_dir = std::string(home) + "/.vibe-fi/playlists";
    } else {
        playlists_dir = "playlists";
    }
    ensure_playlists_dir();
}

void PlaylistManager::ensure_playlists_dir() {
    if (!fs::exists(playlists_dir)) {
        fs::create_directories(playlists_dir);
    }
}

std::string PlaylistManager::get_playlist_path(const std::string& name) {
    return playlists_dir + "/" + name + ".txt";
}

bool PlaylistManager::create_playlist(const std::string& name) {
    std::string path = get_playlist_path(name);
    if (!fs::exists(path)) {
        std::ofstream outfile(path);
        outfile.close();
        return true;
    }
    return false;
}

void PlaylistManager::delete_playlist(const std::string& name) {
    std::string path = get_playlist_path(name);
    if (fs::exists(path)) {
        fs::remove(path);
    }
}

bool PlaylistManager::rename_playlist(const std::string& old_name, const std::string& new_name) {
    if (new_name.empty()) return false;
    
    std::string old_path = get_playlist_path(old_name);
    std::string new_path = get_playlist_path(new_name);
    
    if (!fs::exists(old_path)) return false; // Old doesn't exist
    if (fs::exists(new_path)) return false;  // New already exists
    
    try {
        fs::rename(old_path, new_path);
        return true;
    } catch (...) {
        return false;
    }
}

bool PlaylistManager::move_song(const std::string& src_playlist, int src_index, const std::string& dest_playlist) {
    auto songs = get_playlist_songs(src_playlist);
    if (src_index < 0 || src_index >= songs.size()) return false;
    
    PlaylistSong song_to_move = songs[src_index];
    
    // Add to destination
    if (add_song_to_playlist(dest_playlist, song_to_move)) {
        // Remove from source only if add was successful
        remove_song_from_playlist(src_playlist, src_index);
        return true;
    }
    return false;
}

bool PlaylistManager::add_song_to_playlist(const std::string& playlist_name, const PlaylistSong& song) {
    // Check for duplicates
    auto current_songs = get_playlist_songs(playlist_name);
    for (const auto& s : current_songs) {
        if (s.url == song.url) return false;
    }

    std::string path = get_playlist_path(playlist_name);
    std::ofstream outfile(path, std::ios::app);
    if (outfile.is_open()) {
        outfile << song.title << "|" << song.url << "|" << song.duration << "\n";
        outfile.close();
        return true;
    }
    return false;
}

void PlaylistManager::remove_song_from_playlist(const std::string& playlist_name, int index) {
    auto songs = get_playlist_songs(playlist_name);
    if (index >= 0 && index < songs.size()) {
        songs.erase(songs.begin() + index);
        
        std::string path = get_playlist_path(playlist_name);
        std::ofstream outfile(path); // Truncate file
        if (outfile.is_open()) {
            for (const auto& song : songs) {
                outfile << song.title << "|" << song.url << "|" << song.duration << "\n";
            }
            outfile.close();
        }
    }
}

std::vector<Playlist> PlaylistManager::list_playlists() {
    std::vector<Playlist> playlists;
    if (fs::exists(playlists_dir)) {
        for (const auto& entry : fs::directory_iterator(playlists_dir)) {
            if (entry.path().extension() == ".txt") {
                Playlist pl;
                pl.name = entry.path().stem().string();
                pl.path = entry.path().string();
                
                // Count lines to get song count
                std::ifstream f(pl.path);
                pl.song_count = std::count(std::istreambuf_iterator<char>(f), 
                                           std::istreambuf_iterator<char>(), '\n');
                playlists.push_back(pl);
            }
        }
    }
    return playlists;
}

std::vector<PlaylistSong> PlaylistManager::get_playlist_songs(const std::string& playlist_name) {
    std::vector<PlaylistSong> songs;
    std::string path = get_playlist_path(playlist_name);
    std::ifstream infile(path);
    std::string line;
    
    while (std::getline(infile, line)) {
        if (line.empty()) continue;
        
        size_t last_pipe = line.rfind('|');
        if (last_pipe == std::string::npos) continue;
        
        size_t second_last_pipe = line.rfind('|', last_pipe - 1);
        if (second_last_pipe == std::string::npos) continue;
        
        PlaylistSong song;
        song.title = line.substr(0, second_last_pipe);
        song.url = line.substr(second_last_pipe + 1, last_pipe - second_last_pipe - 1);
        song.duration = line.substr(last_pipe + 1);
        songs.push_back(song);
    }
    
    return songs;
}
