#include "ui.hpp"
#include "utils.hpp"
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
    
    // Autoplay defaults
    autoplay_enabled = true;
    playing_index = -1;
    is_playing_from_playlist = false;
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
        
        // Autoplay check
        if (autoplay_enabled && player.is_idle() && playing_index != -1) {
            play_next();
        }
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
    } else if (mode == AppMode::PLAYLIST_BROWSER) {
        draw_playlists();
    } else if (mode == AppMode::PLAYLIST_VIEW) {
        draw_playlist_view();
    } else if (mode == AppMode::PLAYLIST_SELECT_FOR_ADD) {
        draw_playlist_select_for_add();
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

void UI::draw_playlist_select_for_add() {
    werase(main_win);
    draw_borders(main_win, "SELECT PLAYLIST TO ADD TO");
    
    int height, width;
    getmaxyx(main_win, height, width);
    
    if (playlists.empty()) {
        mvwprintw(main_win, height/2, 2, "No playlists found. Press [N] to create one.");
    } else {
        // Header
        wattron(main_win, A_BOLD | A_UNDERLINE);
        mvwprintw(main_win, 1, 2, "%-20s %10s", "Playlist Name", "Songs");
        wattroff(main_win, A_BOLD | A_UNDERLINE);
        
        for (int i = 0; i < playlists.size(); ++i) {
            int y = i + 2;
            if (y >= height - 2) break;
            
            if (i == selection_index) wattron(main_win, COLOR_PAIR(6));
            
            std::string name = playlists[i].name;
            if (name.length() > 20) name = name.substr(0, 17) + "...";
            
            mvwprintw(main_win, y, 2, "%-20s %10d", name.c_str(), playlists[i].song_count);
            
            if (i == selection_index) wattroff(main_win, COLOR_PAIR(6));
        }
    }
    
    // Always show the option to create new
    mvwprintw(main_win, height - 2, 2, "Press [N] to create a new playlist");
    wrefresh(main_win);
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
    
    int height, width;
    getmaxyx(main_win, height, width);
    
    std::string prompt = "What do you want to listen to?";
    int prompt_x = (width - prompt.length()) / 2;
    int prompt_y = height / 2 - 2;
    
    wattron(main_win, A_BOLD);
    mvwprintw(main_win, prompt_y, prompt_x, "%s", prompt.c_str());
    wattroff(main_win, A_BOLD);
    
    // Draw input box
    int box_width = std::min(60, width - 4);
    int box_x = (width - box_width) / 2;
    int box_y = prompt_y + 2;
    
    mvwprintw(main_win, box_y, box_x - 2, "> ");
    
    wattron(main_win, COLOR_PAIR(6)); // Highlight background for input
    mvwhline(main_win, box_y, box_x, ' ', box_width);
    mvwprintw(main_win, box_y, box_x, "%s", search_query.c_str());
    if (search_query.length() < box_width) {
        waddch(main_win, '_'); // Cursor
    }
    wattroff(main_win, COLOR_PAIR(6));
    
    wrefresh(main_win);
}

void UI::draw_search_results() {
    werase(main_win);
    draw_borders(main_win, "SEARCH RESULTS");
    
    int height, width;
    getmaxyx(main_win, height, width);
    
    if (search_results.empty()) {
        std::string msg = "Searching...";
        mvwprintw(main_win, height/2, (width - msg.length())/2, "%s", msg.c_str());
    } else {
        // Header
        wattron(main_win, A_BOLD | A_UNDERLINE);
        mvwprintw(main_win, 1, 2, "%-4s %-50s %10s", "#", "Title", "Duration");
        wattroff(main_win, A_BOLD | A_UNDERLINE);
        
        for (int i = 0; i < search_results.size(); ++i) {
            int y = i + 2;
            if (y >= height - 1) break;
            
            if (i == selection_index) wattron(main_win, COLOR_PAIR(6));
            
            std::string title = search_results[i].title;
            if (title.length() > 50) title = title.substr(0, 47) + "...";
            
            mvwprintw(main_win, y, 2, "%-4d %-50s %10s", i + 1, title.c_str(), search_results[i].duration.c_str());
            
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
    
    // Center Title
    if (title.length() > width - 4) title = title.substr(0, width - 7) + "...";
    int title_x = (width - title.length()) / 2;
    
    wattron(status_win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(status_win, 1, title_x, "%s", title.c_str());
    wattroff(status_win, COLOR_PAIR(1) | A_BOLD);
    
    double pos = player.get_position();
    double dur = player.get_duration();
    int bar_width = width - 4;
    
    if (dur > 0) {
        int filled = static_cast<int>((pos / dur) * bar_width);
        mvwprintw(status_win, 2, 2, "[");
        wattron(status_win, COLOR_PAIR(2));
        for (int i = 0; i < bar_width - 2; ++i) {
            waddch(status_win, i < filled ? '=' : ' ');
        }
        wattroff(status_win, COLOR_PAIR(2));
        wprintw(status_win, "]");
        
        int min_pos = static_cast<int>(pos) / 60;
        int sec_pos = static_cast<int>(pos) % 60;
        int min_dur = static_cast<int>(dur) / 60;
        int sec_dur = static_cast<int>(dur) % 60;
        
        mvwprintw(status_win, 3, 2, "%02d:%02d / %02d:%02d", min_pos, sec_pos, min_dur, sec_dur);
    }
    
    std::string vol_str = "Vol: " + std::to_string(player.get_volume()) + "%";
    mvwprintw(status_win, 3, width - vol_str.length() - 2, "%s", vol_str.c_str());
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
        if (mode == AppMode::PLAYBACK) {
             std::string auto_str = autoplay_enabled ? "ON" : "OFF";
             mvwprintw(help_win, 1, 2, "[ESC] Quit [SPACE] Pause [Q] Queue [L] Library [S] Search [P] Playlist [R] Replay [O] Autoplay:%s", auto_str.c_str());
        }
        else if (mode == AppMode::LIBRARY_BROWSER)
             mvwprintw(help_win, 1, 2, "[ENTER] Select [BKSP] Up [ESC] Back");
        else if (mode == AppMode::SEARCH_INPUT)
             mvwprintw(help_win, 1, 2, "[ENTER] Search [ESC] Cancel");
        else if (mode == AppMode::SEARCH_RESULTS)
             mvwprintw(help_win, 1, 2, "[ENTER] Play [A] Add to Playlist [S] New Search [ESC] Back");
        else if (mode == AppMode::PLAYLIST_BROWSER)
             mvwprintw(help_win, 1, 2, "[ENTER] View [N] New Playlist [D] Delete [ESC] Back");
        else if (mode == AppMode::PLAYLIST_VIEW)
             mvwprintw(help_win, 1, 2, "[ENTER] Play [D] Remove [ESC] Back");
        else if (mode == AppMode::PLAYLIST_SELECT_FOR_ADD)
             mvwprintw(help_win, 1, 2, "[ENTER] Select [N] New Playlist [ESC] Cancel");
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
        else if (mode == AppMode::PLAYLIST_BROWSER) handle_playlists_input(ch);
        else if (mode == AppMode::PLAYLIST_VIEW) handle_playlist_view_input(ch);
        else if (mode == AppMode::PLAYLIST_SELECT_FOR_ADD) handle_playlist_select_for_add_input(ch);
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
            if (!playing_playlist_name.empty()) {
                current_playlist_name = playing_playlist_name;
                current_playlist_songs = playlist_manager.get_playlist_songs(current_playlist_name);
                selection_index = 0; // Or try to find the current song index?
                set_mode(AppMode::PLAYLIST_VIEW);
            } else if (!search_results.empty()) {
                set_mode(AppMode::SEARCH_RESULTS);
            } else {
                show_message("Not playing from a playlist.");
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
        case 'o': case 'O': 
            autoplay_enabled = !autoplay_enabled; 
            show_message(std::string("Autoplay: ") + (autoplay_enabled ? "ON" : "OFF"));
            break;
        case 'p': case 'P':
            playlists = playlist_manager.list_playlists();
            set_mode(AppMode::PLAYLIST_BROWSER);
            break;
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
                wrefresh(help_win); 
                
                try {
                    std::string stream_url = get_youtube_stream_url(search_results[selection_index].url);
                    player.load(stream_url);
                    last_played_path = stream_url;
                    player.set_property("force-media-title", search_results[selection_index].title);
                    
                    // Set autoplay context
                    playing_index = selection_index;
                    is_playing_from_playlist = false;
                    
                    player.play();
                    set_mode(AppMode::PLAYBACK);
                } catch (const std::exception& e) {
                    show_message(std::string("Cannot play: ") + e.what());
                }
            }
            break;
        case 'a': case 'A':
            if (!search_results.empty()) {
                song_to_add.title = search_results[selection_index].title;
                song_to_add.url = search_results[selection_index].url;
                song_to_add.duration = search_results[selection_index].duration;
                
                playlists = playlist_manager.list_playlists();
                // Always allow entering selection mode so user can create new playlist
                selection_index = 0;
                set_mode(AppMode::PLAYLIST_SELECT_FOR_ADD);
            }
            break;
    }
}

void UI::draw_playlists() {
    werase(main_win);
    draw_borders(main_win, "PLAYLISTS");
    
    int height, width;
    getmaxyx(main_win, height, width);
    
    if (playlists.empty()) {
        mvwprintw(main_win, height/2, 2, "No playlists found. Press [N] to create one.");
    } else {
        // Header
        wattron(main_win, A_BOLD | A_UNDERLINE);
        mvwprintw(main_win, 1, 2, "%-20s %10s", "Playlist Name", "Songs");
        wattroff(main_win, A_BOLD | A_UNDERLINE);
        
        for (int i = 0; i < playlists.size(); ++i) {
            int y = i + 2;
            if (y >= height - 1) break;
            
            if (i == selection_index) wattron(main_win, COLOR_PAIR(6));
            
            std::string name = playlists[i].name;
            if (name.length() > 20) name = name.substr(0, 17) + "...";
            
            mvwprintw(main_win, y, 2, "%-20s %10d", name.c_str(), playlists[i].song_count);
            
            if (i == selection_index) wattroff(main_win, COLOR_PAIR(6));
        }
    }
    wrefresh(main_win);
}

void UI::draw_playlist_view() {
    werase(main_win);
    draw_borders(main_win, "PLAYLIST: " + current_playlist_name);
    
    int height, width;
    getmaxyx(main_win, height, width);
    
    if (current_playlist_songs.empty()) {
        mvwprintw(main_win, height/2, 2, "Playlist is empty.");
    } else {
        // Header
        wattron(main_win, A_BOLD | A_UNDERLINE);
        mvwprintw(main_win, 1, 2, "%-4s %-50s %10s", "#", "Title", "Duration");
        wattroff(main_win, A_BOLD | A_UNDERLINE);
        
        for (int i = 0; i < current_playlist_songs.size(); ++i) {
            int y = i + 2;
            if (y >= height - 1) break;
            
            if (i == selection_index) wattron(main_win, COLOR_PAIR(6));
            
            std::string title = current_playlist_songs[i].title;
            if (title.length() > 50) title = title.substr(0, 47) + "...";
            
            mvwprintw(main_win, y, 2, "%-4d %-50s %10s", i + 1, title.c_str(), current_playlist_songs[i].duration.c_str());
            
            if (i == selection_index) wattroff(main_win, COLOR_PAIR(6));
        }
    }
    wrefresh(main_win);
}

void UI::handle_playlists_input(int ch) {
    switch (ch) {
        case 27: set_mode(AppMode::PLAYBACK); break;
        case KEY_UP: if (selection_index > 0) selection_index--; break;
        case KEY_DOWN: if (selection_index < playlists.size() - 1) selection_index++; break;
        case 'n': case 'N': {
            std::string name = get_user_input("New Playlist Name");
            
            if (name.length() > 0) {
                if (playlist_manager.create_playlist(name)) {
                    playlists = playlist_manager.list_playlists();
                    show_message("Playlist created.");
                } else {
                    show_message("Playlist already exists.");
                }
            }
            break;
        }
        case 'd': case 'D':
            if (!playlists.empty()) {
                playlist_manager.delete_playlist(playlists[selection_index].name);
                playlists = playlist_manager.list_playlists();
                if (selection_index >= playlists.size() && selection_index > 0) selection_index--;
                show_message("Playlist deleted.");
            }
            break;
        case 10: // Enter
            if (!playlists.empty()) {
                current_playlist_name = playlists[selection_index].name;
                current_playlist_songs = playlist_manager.get_playlist_songs(current_playlist_name);
                selection_index = 0;
                set_mode(AppMode::PLAYLIST_VIEW);
            }
            break;
    }
}

void UI::handle_playlist_view_input(int ch) {
    switch (ch) {
        case 27: 
            playlists = playlist_manager.list_playlists();
            set_mode(AppMode::PLAYLIST_BROWSER); 
            break;
        case KEY_UP: if (selection_index > 0) selection_index--; break;
        case KEY_DOWN: if (selection_index < current_playlist_songs.size() - 1) selection_index++; break;
        case 'd': case 'D':
            if (!current_playlist_songs.empty()) {
                playlist_manager.remove_song_from_playlist(current_playlist_name, selection_index);
                current_playlist_songs = playlist_manager.get_playlist_songs(current_playlist_name);
                if (selection_index >= current_playlist_songs.size() && selection_index > 0) selection_index--;
                show_message("Song removed.");
            }
            break;
        case 10: // Enter
            if (!current_playlist_songs.empty()) {
                show_message("Resolving...");
                wrefresh(help_win);
                try {
                    std::string stream_url = get_youtube_stream_url(current_playlist_songs[selection_index].url);
                    player.load(stream_url);
                    last_played_path = stream_url;
                    player.set_property("force-media-title", current_playlist_songs[selection_index].title);
                    playing_playlist_name = current_playlist_name;
                    
                    // Set autoplay context
                    playing_index = selection_index;
                    is_playing_from_playlist = true;
                    
                    player.play();
                    set_mode(AppMode::PLAYBACK);
                } catch (const std::exception& e) {
                    show_message(std::string("Cannot play: ") + e.what());
                }
            }
            break;
    }
}
void UI::handle_playlist_select_for_add_input(int ch) {
    switch (ch) {
        case 27: 
            set_mode(AppMode::SEARCH_RESULTS); 
            break;
        case KEY_UP: if (selection_index > 0) selection_index--; break;
        case KEY_DOWN: if (selection_index < playlists.size() - 1) selection_index++; break;
        case 10: // Enter
            if (!playlists.empty()) {
                if (playlist_manager.add_song_to_playlist(playlists[selection_index].name, song_to_add)) {
                    show_message("Song added to " + playlists[selection_index].name);
                    set_mode(AppMode::SEARCH_RESULTS);
                } else {
                    show_message("Song already in playlist.");
                }
            }
            break;
        case 'n': case 'N': {
            std::string name = get_user_input("New Playlist Name");
            
            if (name.length() > 0) {
                if (playlist_manager.create_playlist(name)) {
                    playlists = playlist_manager.list_playlists();
                    show_message("Playlist created.");
                    // Auto-select the new playlist
                    selection_index = playlists.size() - 1;
                } else {
                    show_message("Playlist already exists.");
                }
            }
            draw_playlist_select_for_add();
            break;
        }
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
    
    std::string instruction = "Press [L] Library  [S] Search  [P] Playlists  [ESC] Quit";
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
    } else if (ch == 'p' || ch == 'P') {
        playlists = playlist_manager.list_playlists();
        set_mode(AppMode::PLAYLIST_BROWSER);
    } else if (ch == 27 || ch == 'q' || ch == 'Q') { // ESC or Q
        running = false;
    }
}

void UI::play_next() {
    if (playing_index == -1) return;
    
    int next_index = playing_index + 1;
    std::string next_url;
    std::string next_title;
    
    if (is_playing_from_playlist) {
        if (next_index < current_playlist_songs.size()) {
            next_url = current_playlist_songs[next_index].url;
            next_title = current_playlist_songs[next_index].title;
        } else {
            // End of playlist
            playing_index = -1;
            show_message("End of playlist.");
            return;
        }
    } else {
        if (next_index < search_results.size()) {
            next_url = search_results[next_index].url;
            next_title = search_results[next_index].title;
        } else {
            // End of search results
            playing_index = -1;
            show_message("End of results.");
            return;
        }
    }
    
    try {
        show_message("Autoplaying next: " + next_title);
        wrefresh(help_win);
        
        std::string stream_url = get_youtube_stream_url(next_url);
        player.load(stream_url);
        last_played_path = stream_url;
        player.set_property("force-media-title", next_title);
        player.play();
        
        playing_index = next_index;
    } catch (const std::exception& e) {
        show_message("Autoplay failed: " + std::string(e.what()));
        playing_index = -1; // Stop autoplay on error
    }
}

std::string UI::get_user_input(const std::string& prompt) {
    int height, width;
    getmaxyx(stdscr, height, width);
    
    int win_h = 5;
    int win_w = 40;
    int start_y = (height - win_h) / 2;
    int start_x = (width - win_w) / 2;
    
    WINDOW* input_win = newwin(win_h, win_w, start_y, start_x);
    wbkgd(input_win, COLOR_PAIR(1)); 
    box(input_win, 0, 0);
    
    mvwprintw(input_win, 0, 2, " %s ", prompt.c_str());
    mvwprintw(input_win, 2, 2, "> ");
    wrefresh(input_win);
    
    curs_set(1);
    std::string input;
    int ch;
    
    while (true) {
        ch = wgetch(input_win);
        if (ch == 27) { // ESC
            input = ""; // Cancel
            break;
        } else if (ch == 10) { // Enter
            break;
        } else if (ch == KEY_BACKSPACE || ch == 127) {
            if (!input.empty()) {
                input.pop_back();
                mvwprintw(input_win, 2, 4, "%s ", input.c_str()); // Clear last char
                wmove(input_win, 2, 4 + input.length());
                wrefresh(input_win);
            }
        } else if (isprint(ch)) {
            if (input.length() < win_w - 6) {
                input += (char)ch;
                mvwprintw(input_win, 2, 4, "%s", input.c_str());
                wrefresh(input_win);
            }
        }
    }
    
    curs_set(0);
    delwin(input_win);
    
    // Force full redraw after popup
    clear();
    refresh();
    draw(); 
    
    return input;
}

