#include "ui.hpp"
#include <ncurses.h>
#include <cmath>
#include <vector>
#include <chrono>
#include <thread>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

UI::UI(Player& p) : player(p), running(true), mode(AppMode::PLAYBACK), selection_index(0), scroll_offset(0) {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    timeout(100); 
    
    start_color();
    use_default_colors();
    
    // Define colors
    init_pair(1, COLOR_CYAN, -1);    // Borders/Text
    init_pair(2, COLOR_GREEN, -1);   // Progress/Active
    init_pair(3, COLOR_MAGENTA, -1); // Visualizer
    init_pair(4, COLOR_RED, -1);     // Alerts/Help
    init_pair(5, COLOR_BLUE, -1);    // Background elements
    init_pair(6, COLOR_BLACK, COLOR_CYAN); // Selected item

    refresh(); // Refresh stdscr before creating windows
    
    // Initialize library
    current_path = library.get_home_music_dir();
    library_items = library.list_directory(current_path);
    library_items = library.list_directory(current_path);
}

UI::~UI() {
    if (visualizer_win) delwin(visualizer_win);
    if (status_win) delwin(status_win);
    if (help_win) delwin(help_win);
    if (main_win) delwin(main_win);
    endwin();
}

WINDOW* UI::create_window(int height, int width, int starty, int startx) {
    WINDOW* local_win = newwin(height, width, starty, startx);
    return local_win;
}

void UI::draw_borders(WINDOW* win, const std::string& title) {
    wattron(win, COLOR_PAIR(1));
    box(win, 0, 0);
    if (!title.empty()) {
        mvwprintw(win, 0, 2, " %s ", title.c_str());
    }
    wattroff(win, COLOR_PAIR(1));
    wrefresh(win);
}

void UI::set_mode(AppMode new_mode) {
    mode = new_mode;
    selection_index = 0;
    scroll_offset = 0;
    clear();
    refresh();
}

void UI::run() {
    int height, width;
    getmaxyx(stdscr, height, width);

    // Layout calculation
    int help_h = 3;
    int status_h = 5;
    int main_h = height - status_h - help_h;
    
    visualizer_win = create_window(main_h, width, 0, 0); // Reused as main view area
    main_win = create_window(main_h, width, 0, 0);       // Overlaps with visualizer
    status_win = create_window(status_h, width, main_h, 0);
    help_win = create_window(help_h, width, main_h + status_h, 0);

    while (running) {
        // Check for resize (simple handling)
        int new_h, new_w;
        getmaxyx(stdscr, new_h, new_w);
        if (new_h != height || new_w != width) {
            height = new_h;
            width = new_w;
            main_h = height - status_h - help_h;
            wresize(visualizer_win, main_h, width);
            wresize(main_win, main_h, width);
            wresize(status_win, status_h, width);
            mvwin(status_win, main_h, 0);
            wresize(help_win, help_h, width);
            mvwin(help_win, height - help_h, 0);
            clear();
            refresh();
        }

        draw();
        handle_input();
    }
}

void UI::draw() {
    if (mode == AppMode::PLAYBACK) {
        draw_playback();
    } else if (mode == AppMode::LIBRARY_BROWSER) {
        draw_library();
    } else if (mode == AppMode::SEARCH_INPUT) {
        draw_search_input();
    } else if (mode == AppMode::SEARCH_RESULTS) {
        draw_search_results();
    } else if (mode == AppMode::INTRO) {
        draw_intro();
    }
    
    update_status();
    update_help();
}

void UI::draw_playback() {
    update_visualizer();
}

void UI::update_visualizer() {
    werase(visualizer_win);
    draw_borders(visualizer_win, "VIBE-FI VISUALIZER");
    
    int height, width;
    getmaxyx(visualizer_win, height, width);
    
    // Inner drawing area
    int draw_h = height - 2;
    int draw_w = width - 2;
    int bar_width = 3; // Wider bars
    int num_bars = draw_w / bar_width;
    
    static std::vector<int> bars(num_bars, 0);
    
    for (int i = 0; i < num_bars; ++i) {
        if (player.is_playing() && !player.is_paused() && !player.is_idle()) {
            // Reduce max height to 60% of available space for a cleaner look
            int max_h = static_cast<int>(draw_h * 0.6);
            if (max_h < 1) max_h = 1;
            
            int target = rand() % max_h;
            if (bars[i] < target) bars[i] += 2;
            else if (bars[i] > target) bars[i] -= 1;
            if (bars[i] < 0) bars[i] = 0;
            if (bars[i] >= draw_h) bars[i] = draw_h - 1;
        } else {
            if (bars[i] > 0) bars[i]--;
        }
    }
    
    wattron(visualizer_win, COLOR_PAIR(3));
    int center_y = height / 2;
    for (int i = 0; i < num_bars; ++i) {
        int bar_height = bars[i];
        int half_height = bar_height / 2;
        
        int start_y = center_y - half_height;
        int end_y = center_y + half_height;
        
        // Clamp to avoid drawing over borders
        if (start_y < 1) start_y = 1;
        if (end_y > height - 2) end_y = height - 2;
        
        for (int y = start_y; y <= end_y; ++y) {
            for (int k = 0; k < bar_width - 1; ++k) {
                 mvwaddch(visualizer_win, y, (i * bar_width) + 1 + k, ACS_CKBOARD); 
            }
        }
    }
    wattroff(visualizer_win, COLOR_PAIR(3));
    
    wrefresh(visualizer_win);
}

void UI::draw_library() {
    werase(main_win);
    draw_borders(main_win, "LIBRARY: " + current_path);
    
    int height, width;
    getmaxyx(main_win, height, width);
    int list_h = height - 2;
    
    for (int i = 0; i < list_h && (i + scroll_offset) < library_items.size(); ++i) {
        int idx = i + scroll_offset;
        const auto& item = library_items[idx];
        
        if (idx == selection_index) {
            wattron(main_win, COLOR_PAIR(6));
        }
        
        std::string display_name = (item.is_directory ? "[DIR] " : "      ") + item.name;
        if (!item.is_directory && !item.duration.empty()) {
            display_name += " (" + item.duration + ")";
        }
        if (display_name.length() > width - 4) display_name = display_name.substr(0, width - 4);
        mvwprintw(main_win, i + 1, 2, "%s", display_name.c_str());
        
        if (idx == selection_index) {
            wattroff(main_win, COLOR_PAIR(6));
        }
    }
    wrefresh(main_win);
}

void UI::draw_search_input() {
    werase(main_win);
    draw_borders(main_win, "SEARCH YOUTUBE");
    mvwprintw(main_win, 2, 2, "Enter Query: %s_", search_query.c_str());
    wrefresh(main_win);
}

void UI::draw_search_results() {
    werase(main_win);
    draw_borders(main_win, "SEARCH RESULTS");
    
    int height, width;
    getmaxyx(main_win, height, width);
    
    if (search_results.empty()) {
        mvwprintw(main_win, 2, 2, "Searching...");
    } else {
        for (int i = 0; i < search_results.size(); ++i) {
            if (i == selection_index) wattron(main_win, COLOR_PAIR(6));
            std::string display_title = search_results[i].title;
            if (!search_results[i].duration.empty()) {
                display_title += " (" + search_results[i].duration + ")";
            }
            if (display_title.length() > width - 4) display_title = display_title.substr(0, width - 4);
            mvwprintw(main_win, i + 1, 2, "%s", display_title.c_str());
            if (i == selection_index) wattroff(main_win, COLOR_PAIR(6));
        }
    }
    wrefresh(main_win);
}



void UI::update_status() {
    werase(status_win);
    draw_borders(status_win, "NOW PLAYING");
    
    int height, width;
    getmaxyx(status_win, height, width);
    
    std::string title = player.get_metadata("media-title");
    if (title.empty()) {
        std::string filename = player.get_metadata("filename");
        if (filename.empty()) {
            title = "Not Playing";
        } else {
            title = filename;
        }
    }
    
    wattron(status_win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(status_win, 1, 2, "Track: %.50s", title.c_str());
    wattroff(status_win, COLOR_PAIR(1) | A_BOLD);
    
    double pos = player.get_position();
    double dur = player.get_duration();
    int bar_width = width - 4;
    
    if (dur > 0) {
        int filled = static_cast<int>((pos / dur) * bar_width);
        mvwprintw(status_win, 2, 2, "[");
        wattron(status_win, COLOR_PAIR(2));
        for (int i = 0; i < bar_width - 2; ++i) {
            waddch(status_win, i < filled ? '=' : '-');
        }
        wattroff(status_win, COLOR_PAIR(2));
        wprintw(status_win, "]");
        
        int min_pos = static_cast<int>(pos) / 60;
        int sec_pos = static_cast<int>(pos) % 60;
        int min_dur = static_cast<int>(dur) / 60;
        int sec_dur = static_cast<int>(dur) % 60;
        
        mvwprintw(status_win, 3, 2, "%02d:%02d / %02d:%02d", min_pos, sec_pos, min_dur, sec_dur);
    }
    
    mvwprintw(status_win, 3, width - 15, "Vol: %d%%", player.get_volume());
    wrefresh(status_win);
}

void UI::update_help() {
    werase(help_win);
    auto now = std::chrono::steady_clock::now();
    if (!message.empty() && std::chrono::duration_cast<std::chrono::seconds>(now - message_time).count() < 3) {
        wattron(help_win, COLOR_PAIR(4) | A_BOLD);
        mvwprintw(help_win, 1, 2, "MSG: %s", message.c_str());
        wattroff(help_win, COLOR_PAIR(4) | A_BOLD);
    } else {
        wattron(help_win, COLOR_PAIR(4));
        if (mode == AppMode::PLAYBACK)
            mvwprintw(help_win, 1, 2, "[SPACE] Play/Pause [Q] Queue [L] Library [S] Search [R] Replay");
        else if (mode == AppMode::LIBRARY_BROWSER)
             mvwprintw(help_win, 1, 2, "[ENTER] Select [BKSP] Up [ESC] Back");
        else if (mode == AppMode::SEARCH_INPUT)
             mvwprintw(help_win, 1, 2, "[ENTER] Search [ESC] Cancel");
        else if (mode == AppMode::SEARCH_RESULTS)
             mvwprintw(help_win, 1, 2, "[ENTER] Play [S] New Search [ESC] Back");
        else
             mvwprintw(help_win, 1, 2, "[ENTER] Play [ESC] Back");
        
        if (mode == AppMode::INTRO)
             mvwprintw(help_win, 1, 2, "Welcome! Press [ENTER] to browse library.");
        wattroff(help_win, COLOR_PAIR(4));
    }
    wrefresh(help_win);
}

void UI::show_message(const std::string& msg) {
    message = msg;
    message_time = std::chrono::steady_clock::now();
    update_help();
}

void UI::handle_input() {
    int ch = getch();
    if (ch == ERR) return;

    try {
        if (mode == AppMode::PLAYBACK) handle_playback_input(ch);
        else if (mode == AppMode::LIBRARY_BROWSER) handle_library_input(ch);
        else if (mode == AppMode::SEARCH_INPUT) handle_search_input_input(ch);
        else if (mode == AppMode::SEARCH_RESULTS) handle_search_results_input(ch);
        else if (mode == AppMode::INTRO) handle_intro_input(ch);
    } catch (const std::exception& e) {
        show_message(std::string("Error: ") + e.what());
    }
}

void UI::handle_playback_input(int ch) {
    switch (ch) {
        case 27: running = false; break;
        case ' ': player.toggle_pause(); break;
        case 'l': set_mode(AppMode::LIBRARY_BROWSER); break;
        case 's': search_query = ""; set_mode(AppMode::SEARCH_INPUT); break;
        case 'q': case 'Q':
            if (!search_results.empty()) {
                set_mode(AppMode::SEARCH_RESULTS);
            }
            break;
        case 'r': case 'R': 
            if (!last_played_path.empty()) {
                player.load(last_played_path);
                player.play();
                show_message("Replaying...");
            }
            break;
        case KEY_LEFT: player.seek(-5.0); break;
        case KEY_RIGHT: player.seek(5.0); break;
        case '+': case '=': player.set_volume(player.get_volume() + 5); break;
        case '-': case '_': player.set_volume(player.get_volume() - 5); break;
    }
}

void UI::handle_library_input(int ch) {
    switch (ch) {
        case 27: set_mode(AppMode::PLAYBACK); break; 
        case KEY_UP: 
            if (selection_index > 0) {
                selection_index--;
                if (selection_index < scroll_offset) scroll_offset = selection_index;
            }
            break;
        case KEY_DOWN:
            if (selection_index < library_items.size() - 1) {
                selection_index++;
                int height, w; 
                getmaxyx(main_win, height, w);
                if (selection_index >= scroll_offset + height - 2) scroll_offset++;
            }
            break;
        case KEY_BACKSPACE:
        case 127:
            if (current_path != "/") {
                current_path = fs::path(current_path).parent_path().string();
                library_items = library.list_directory(current_path);
                selection_index = 0; scroll_offset = 0;
            }
            break;
        case 10: // Enter
            if (library_items.empty()) break;
            auto& item = library_items[selection_index];
            if (item.is_directory) {
                current_path = item.path;
                library_items = library.list_directory(current_path);
                selection_index = 0; scroll_offset = 0;
            } else {
                player.load(item.path);
                last_played_path = item.path;
                player.set_property("force-media-title", "");
                player.play();
                set_mode(AppMode::PLAYBACK);
            }
            break;
    }
}

void UI::handle_search_input_input(int ch) {
    if (ch == 27) {
        set_mode(AppMode::PLAYBACK);
    } else if (ch == 10) { // Enter
        set_mode(AppMode::SEARCH_RESULTS);
        show_message("Searching...");
        draw(); // Force draw to show "Searching..."
        try {
            search_results = search_youtube(search_query);
            if (search_results.empty()) show_message("No results found.");
        } catch (const std::exception& e) {
            show_message("Search failed.");
        }
    } else if (ch == KEY_BACKSPACE || ch == 127) {
        if (!search_query.empty()) search_query.pop_back();
    } else if (isprint(ch)) {
        search_query += (char)ch;
    }
}

void UI::handle_search_results_input(int ch) {
    switch (ch) {
        case 27: set_mode(AppMode::PLAYBACK); break;
        case 's': case 'S':
            search_query = "";
            selection_index = 0;
            set_mode(AppMode::SEARCH_INPUT);
            break;
        case KEY_UP: if (selection_index > 0) selection_index--; break;
        case KEY_DOWN: if (selection_index < search_results.size() - 1) selection_index++; break;
        case 10: // Enter
            if (!search_results.empty()) {
                show_message("Resolving...");
                // Force a quick draw to show the message? 
                // ncurses doesn't update until next loop usually, but we can try wrefresh
                wrefresh(help_win); 
                
                try {
                    std::string stream_url = get_youtube_stream_url(search_results[selection_index].url);
                    player.load(stream_url);
                    last_played_path = stream_url;
                    player.set_property("force-media-title", search_results[selection_index].title);
                    player.play();
                    set_mode(AppMode::PLAYBACK);
                } catch (const std::exception& e) {
                    // Stay in search results, just show error
                    show_message(std::string("Cannot play: ") + e.what());
                }
            }
            break;
    }
}

void UI::draw_intro() {
    werase(main_win);
    draw_borders(main_win, "");
    
    int height, width;
    getmaxyx(main_win, height, width);
    
    std::vector<std::string> ascii_art = {
        " __      __  ___   ____    ______           ______   __ ",
        " \\ \\    / / |_ _| |  _ \\  |  ____|         |  ____| |  |",
        "  \\ \\  / /   | |  | |_) | | |__     _____  | |__    |  |",
        "   \\ \\/ /    | |  |  _ <  |  __|   |_____| |  __|   |  |",
        "    \\  /     | |  | |_) | | |____          | |      |  |",
        "     \\/     |___| |____/  |______|         |_|      |__|"
    };
    
    int start_y = (height - ascii_art.size()) / 2 - 2;
    wattron(main_win, COLOR_PAIR(1) | A_BOLD);
    for (int i = 0; i < ascii_art.size(); ++i) {
        int start_x = (width - ascii_art[i].length()) / 2;
        if (start_x < 0) start_x = 0;
        mvwprintw(main_win, start_y + i, start_x, "%s", ascii_art[i].c_str());
    }
    wattroff(main_win, COLOR_PAIR(1) | A_BOLD);
    
    std::string welcome = "Welcome to Vibe-Fi";
    mvwprintw(main_win, start_y + ascii_art.size() + 2, (width - welcome.length()) / 2, "%s", welcome.c_str());
    
    std::string instruction = "Press [L] Library  [S] Search  [ESC] Quit";
    mvwprintw(main_win, start_y + ascii_art.size() + 4, (width - instruction.length()) / 2, "%s", instruction.c_str());
    
    wrefresh(main_win);
}

void UI::handle_intro_input(int ch) {
    if (ch == 10) { // Enter
        set_mode(AppMode::LIBRARY_BROWSER);
    } else if (ch == 'l' || ch == 'L') {
        set_mode(AppMode::LIBRARY_BROWSER);
    } else if (ch == 's' || ch == 'S') {
        search_query = "";
        set_mode(AppMode::SEARCH_INPUT);
    } else if (ch == 27 || ch == 'q' || ch == 'Q') { // ESC or Q
        running = false;
    }
}

