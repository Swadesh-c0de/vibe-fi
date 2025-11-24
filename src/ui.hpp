#ifndef UI_HPP
#define UI_HPP

#include "player.hpp"
#include "search.hpp"
#include "library.hpp"
#include "utils.hpp"
#include <string>
#include <vector>
#include <ncurses.h>
#include <chrono>

enum class AppMode {
    PLAYBACK,
    LIBRARY_BROWSER,
    SEARCH_INPUT,
    SEARCH_RESULTS,
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

    // State
    Library library;
    std::vector<LibraryItem> library_items;
    std::vector<SearchResult> search_results;

    int selection_index;
    int scroll_offset;
    std::string search_query;
    std::string current_path;
    std::string last_played_path;

    void draw();
    void draw_playback();
    void draw_library();
    void draw_search_input();
    void draw_search_results();
    void draw_intro();
    
    void draw_borders(WINDOW* win, const std::string& title);
    
    void handle_input();
    void handle_playback_input(int ch);
    void handle_library_input(int ch);
    void handle_search_input_input(int ch);
    void handle_search_results_input(int ch);
    void handle_intro_input(int ch);


    void update_visualizer();
    void update_status();
    void update_help();
    
    // Helper to create a window with a border
    WINDOW* create_window(int height, int width, int starty, int startx);

    std::string message;
    std::chrono::steady_clock::time_point message_time;
};

#endif // UI_HPP
