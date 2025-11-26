#pragma once

#include <string>
#include <vector>
#include <filesystem>

struct PlaylistSong {
    std::string title;
    std::string url;
    std::string duration;
};

struct Playlist {
    std::string name;
    std::string path;
    int song_count;
};

class PlaylistManager {
public:
    PlaylistManager();
    
    // Core actions
    bool create_playlist(const std::string& name);
    void delete_playlist(const std::string& name);
    bool add_song_to_playlist(const std::string& playlist_name, const PlaylistSong& song);
    void remove_song_from_playlist(const std::string& playlist_name, int index);
    
    // Data retrieval
    std::vector<Playlist> list_playlists();
    std::vector<PlaylistSong> get_playlist_songs(const std::string& playlist_name);
    
private:
    std::string playlists_dir;
    void ensure_playlists_dir();
    std::string get_playlist_path(const std::string& name);
};
