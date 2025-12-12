#ifndef UI_HPP
#define UI_HPP
#pragma once
#include "player.hpp"
#include "library.hpp"
#include "search.hpp"
#include "playlist_manager.hpp"
#include "lyrics.hpp"
#include <string>
#include <vector>
#include <ncurses.h>
#include <chrono>

enum class AppMode {
    PLAYBACK,
    LIBRARY_BROWSER,
    SEARCH_INPUT,
    SEARCH_RESULTS,
    PLAYLIST_BROWSER,
    PLAYLIST_VIEW,
    PLAYLIST_SELECT_FOR_ADD,
    PLAYLIST_SELECT_FOR_MOVE,
    LYRICS_VIEW,
    INTRO
};

class UI {
public:
    UI(Player& player);
    ~UI();
    
    void run();
    void show_message(const std::string& msg);
    void set_mode(AppMode mode);

private:
    Player& player;
    bool running;
    AppMode mode;
    
    // Windows
    WINDOW* main_win; // Used for browser/search
    WINDOW* visualizer_win;
    WINDOW* status_win;
    WINDOW* help_win;
    WINDOW* lyrics_win;

    // State
    Library library;
    PlaylistManager playlist_manager;
    LyricsManager lyrics_manager;
    std::vector<LibraryItem> library_items;
    std::vector<SearchResult> search_results;

    int selection_index;
    int scroll_offset;
    std::string search_query;
    std::string current_path;
    
    std::vector<Playlist> playlists;
    std::string current_playlist_name;
    std::string playing_playlist_name;
    std::vector<PlaylistSong> current_playlist_songs;
    std::vector<PlaylistSong> preview_songs; // For side-by-side view
    PlaylistSong song_to_add;
    
    LyricsData current_lyrics_data;
    int lyrics_scroll_offset;
    bool lyrics_auto_scroll;
    
    std::string message;
    std::chrono::steady_clock::time_point message_time;
    
    std::string last_played_path;
    
    // Autoplay state
    bool autoplay_enabled;
    int playing_index;
    bool is_playing_from_playlist;

    void draw();
    void draw_playback();
    void draw_library();
    void draw_search_input();
    void draw_search_results();
    void draw_playlists();
    void draw_playlist_view();
    void draw_playlist_select_for_add();
    void draw_lyrics();
    void draw_intro();
    
    // State for moving songs
    int song_to_move_index;
    std::string song_to_move_origin_playlist;

    // Helpers
    void update_preview_songs();
    void fetch_current_lyrics(std::string title_override = "");
    void draw_borders(WINDOW* win, const std::string& title);
    
    void handle_input();
    void handle_playback_input(int ch);
    void handle_library_input(int ch);
    void handle_search_input_input(int ch);
    void handle_search_results_input(int ch);
    void handle_playlists_input(int ch);
    void handle_playlist_view_input(int ch);
    void handle_playlist_select_for_add_input(int ch);
    void handle_playlist_select_for_move_input(int ch);
    void handle_lyrics_input(int ch);
    void handle_intro_input(int ch);


    void update_visualizer();
    void update_status();
    void update_help();
    
    void play_next();
    
    // Helper to create a window with a border
    WINDOW* create_window(int height, int width, int starty, int startx);
    
    // Helper for user input
    std::string get_user_input(const std::string& prompt);

};

#endif // UI_HPP
