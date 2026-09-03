#include "playlist_manager.hpp"
#include "utils.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <sstream>

namespace fs = std::filesystem;

PlaylistManager::PlaylistManager() {
    playlists_dir = get_vibe_dir() + "/playlists";
    ensure_playlists_dir();
}

void PlaylistManager::ensure_playlists_dir() {
    std::error_code ec;
    if (!fs::exists(playlists_dir, ec)) {
        fs::create_directories(playlists_dir, ec);
    }
}

std::string PlaylistManager::get_playlist_path(const std::string& name) {
    // Sanitize playlist name to prevent directory traversal
    std::string safe_name;
    for (char c : name) {
        if (c != '/' && c != '\\' && c != '\0') {
            safe_name += c;
        }
    }
    if (safe_name.empty()) safe_name = "unnamed";
    return playlists_dir + "/" + safe_name + ".txt";
}

bool PlaylistManager::create_playlist(const std::string& name) {
    if (name.empty()) return false;
    std::string path = get_playlist_path(name);
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        std::ofstream outfile(path);
        return outfile.good();
    }
    return false;
}

void PlaylistManager::delete_playlist(const std::string& name) {
    std::string path = get_playlist_path(name);
    std::error_code ec;
    if (fs::exists(path, ec)) {
        fs::remove(path, ec);
    }
}

bool PlaylistManager::rename_playlist(const std::string& old_name, const std::string& new_name) {
    if (new_name.empty() || old_name == new_name) return false;

    std::string old_path = get_playlist_path(old_name);
    std::string new_path = get_playlist_path(new_name);

    std::error_code ec;
    if (!fs::exists(old_path, ec)) return false;
    if (fs::exists(new_path, ec)) return false;

    fs::rename(old_path, new_path, ec);
    return !ec;
}

bool PlaylistManager::move_song(const std::string& src_playlist, int src_index, const std::string& dest_playlist) {
    auto songs = get_playlist_songs(src_playlist);
    if (src_index < 0 || src_index >= static_cast<int>(songs.size())) return false;

    PlaylistSong song_to_move = songs[src_index];

    if (add_song_to_playlist(dest_playlist, song_to_move)) {
        remove_song_from_playlist(src_playlist, src_index);
        return true;
    }
    return false;
}

bool PlaylistManager::add_song_to_playlist(const std::string& playlist_name, const PlaylistSong& song) {
    auto current_songs = get_playlist_songs(playlist_name);
    for (const auto& s : current_songs) {
        if (s.url == song.url) return false; // Avoid duplicate URLs
    }

    std::string path = get_playlist_path(playlist_name);
    std::ofstream outfile(path, std::ios::app);
    if (outfile.is_open()) {
        std::string dur = song.duration.empty() ? "--:--" : song.duration;
        outfile << song.title << "|" << song.url << "|" << dur << "\n";
        return true;
    }
    return false;
}

void PlaylistManager::remove_song_from_playlist(const std::string& playlist_name, int index) {
    auto songs = get_playlist_songs(playlist_name);
    if (index >= 0 && index < static_cast<int>(songs.size())) {
        songs.erase(songs.begin() + index);

        std::string path = get_playlist_path(playlist_name);
        std::ofstream outfile(path, std::ios::trunc);
        if (outfile.is_open()) {
            for (const auto& song : songs) {
                outfile << song.title << "|" << song.url << "|" << song.duration << "\n";
            }
        }
    }
}

std::vector<Playlist> PlaylistManager::list_playlists() {
    std::vector<Playlist> playlists;
    std::error_code ec;
    if (fs::exists(playlists_dir, ec)) {
        for (const auto& entry : fs::directory_iterator(playlists_dir, ec)) {
            if (entry.path().extension() == ".txt") {
                Playlist pl;
                pl.name = entry.path().stem().string();
                pl.path = entry.path().string();
                pl.song_count = 0;

                std::ifstream f(pl.path);
                std::string line;
                while (std::getline(f, line)) {
                    if (!line.empty()) {
                        pl.song_count++;
                    }
                }
                playlists.push_back(pl);
            }
        }
    }

    std::sort(playlists.begin(), playlists.end(), [](const Playlist& a, const Playlist& b) {
        return a.name < b.name;
    });

    return playlists;
}

std::vector<PlaylistSong> PlaylistManager::get_playlist_songs(const std::string& playlist_name) {
    std::vector<PlaylistSong> songs;
    std::string path = get_playlist_path(playlist_name);
    std::ifstream infile(path);
    if (!infile.is_open()) return songs;

    std::string line;
    while (std::getline(infile, line)) {
        if (line.empty()) continue;

        size_t last_pipe = line.rfind('|');
        if (last_pipe == std::string::npos) continue;

        size_t second_last_pipe = line.rfind('|', last_pipe - 1);
        if (second_last_pipe == std::string::npos) continue;

        PlaylistSong song;
        song.title = sanitize_text(line.substr(0, second_last_pipe));
        song.url = line.substr(second_last_pipe + 1, last_pipe - second_last_pipe - 1);
        song.duration = line.substr(last_pipe + 1);

        if (!song.title.empty() && !song.url.empty()) {
            songs.push_back(song);
        }
    }

    return songs;
}

bool PlaylistManager::export_to_m3u(const std::string& playlist_name, const std::string& out_path) {
    std::vector<PlaylistSong> songs = get_playlist_songs(playlist_name);
    if (songs.empty()) return false;

    std::ofstream out(out_path);
    if (!out.is_open()) return false;

    out << "#EXTM3U\n";
    for (const auto& song : songs) {
        out << "#EXTINF:-1," << song.title << "\n";
        out << song.url << "\n";
    }

    return true;
}
