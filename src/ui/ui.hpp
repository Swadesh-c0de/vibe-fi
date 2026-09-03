#ifndef UI_HPP
#define UI_HPP

#include "player.hpp"
#include "library.hpp"
#include "search.hpp"
#include "playlist_manager.hpp"
#include "lyrics.hpp"
#include "discord_rpc.hpp"
#include "visualizer.hpp"
#include <string>
#include <vector>
#include <ncurses.h>
#include <chrono>
#include <memory>

struct Theme {
    std::string name;
    short border_color;
    short progress_color;
    short visualizer_color;
    short alert_color;
    short bg_color;
    short selected_bg_color;
    short selected_fg_color;
};

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
    QUEUE_VIEW,
    INTRO
};

class UI {
public:
    UI(Player& player);
    ~UI();
    
    // Disable copy
    UI(const UI&) = delete;
    UI& operator=(const UI&) = delete;

    void run();
    void show_message(const std::string& msg);
    void set_mode(AppMode mode);
    void set_initial_queue(const std::vector<SearchResult>& results);

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
    std::unique_ptr<DiscordRPC> discord_rpc;
    std::vector<LibraryItem> library_items;
    std::vector<SearchResult> search_results;

    int selection_index;
    int scroll_offset;
    std::string search_query;
    std::string current_path;
    
    VisualizerMode current_visualizer_mode;
    std::vector<Theme> themes;
    int current_theme_idx;
    
    std::vector<Playlist> playlists;
    std::string current_playlist_name;
    std::string playing_playlist_name;
    std::vector<PlaylistSong> current_playlist_songs;
    std::vector<PlaylistSong> preview_songs;
    PlaylistSong song_to_add;
    
    LyricsData current_lyrics_data;
    std::string current_lyrics_title;
    Visualizer visualizer;
    int lyrics_scroll_offset;
    bool lyrics_auto_scroll;
    
    std::string message;
    std::chrono::steady_clock::time_point message_time;
    
    std::string last_played_path;
    
    // Autoplay & queue state
    bool autoplay_enabled;
    int playing_index;
    bool is_playing_from_playlist;
    
    std::vector<PlaylistSong> play_queue;
    int queue_index;

    // Drawing methods
    void draw();
    void draw_playback();
    void draw_library();
    void draw_search_input();
    void draw_search_results();
    void draw_playlists();
    void draw_playlist_view();
    void draw_playlist_select_for_add();
    void draw_lyrics();
    void draw_queue();
    void draw_intro();
    
    // State for moving songs
    int song_to_move_index;
    std::string song_to_move_origin_playlist;

    void save_state();
    void load_state();
    
    // Helpers
    void update_preview_songs();
    void fetch_current_lyrics(std::string title_override = "", std::string url_override = "");
    void draw_borders(WINDOW* win, const std::string& title);
    
    // Input handling
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
    void handle_queue_input(int ch);
    void handle_intro_input(int ch);

    void update_visualizer();
    void update_status();
    void update_help();
    
    void play_next();
    void play_previous();
    
    WINDOW* create_window(int height, int width, int starty, int startx);
    
    void load_themes();
    void apply_theme();
    void cycle_theme();
    void cycle_visualizer();
    
    int last_key;
    std::string get_user_input(const std::string& prompt);
};

#endif // UI_HPP
