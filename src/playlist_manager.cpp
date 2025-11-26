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
        
        size_t first_pipe = line.find('|');
        if (first_pipe == std::string::npos) continue;
        
        size_t second_pipe = line.find('|', first_pipe + 1);
        if (second_pipe == std::string::npos) continue;
        
        PlaylistSong song;
        song.title = line.substr(0, first_pipe);
        song.url = line.substr(first_pipe + 1, second_pipe - first_pipe - 1);
        song.duration = line.substr(second_pipe + 1);
        songs.push_back(song);
    }
    
    return songs;
}
