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

UI::UI(Player& p) : player(p), running(true), mode(AppMode::PLAYBACK), selection_index(0), scroll_offset(0), lyrics_scroll_offset(0), lyrics_auto_scroll(true) {
    set_escdelay(25);
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
    if (lyrics_win) delwin(lyrics_win);
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
    
    if (mode == AppMode::PLAYLIST_BROWSER) {
        update_preview_songs();
    }
    
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
    
    // Split main area: Top 40% for visualizer, Bottom 60% for lyrics
    int viz_h = static_cast<int>(main_h * 0.4);
    int lyrics_h = main_h - viz_h;

    visualizer_win = create_window(viz_h, width, 0, 0); 
    lyrics_win = create_window(lyrics_h, width, viz_h, 0);
    main_win = create_window(main_h, width, 0, 0);       // Overlaps, used for other modes
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
            int viz_h = static_cast<int>(main_h * 0.4);
            int lyrics_h = main_h - viz_h;
            
            wresize(visualizer_win, viz_h, width);
            wresize(lyrics_win, lyrics_h, width);
            mvwin(lyrics_win, viz_h, 0);
            
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
    } else if (mode == AppMode::LYRICS_VIEW) {
        draw_lyrics();
    }
    
    update_status();
    update_help();
}

void UI::draw_playback() {
    update_visualizer();
    draw_lyrics(); // Draw lyrics in the bottom window
}

void UI::update_visualizer() {
    werase(visualizer_win);
    draw_borders(visualizer_win, "VISUALIZER");
    
    int height, width;
    getmaxyx(visualizer_win, height, width);
    
    // Inner drawing area
    int draw_h = height - 2;
    int draw_w = width - 2;
    int bar_width = 2; 
    int num_bars = draw_w / bar_width;
    
    static std::vector<int> bars(num_bars, 0);
    if (bars.size() != num_bars) bars.resize(num_bars, 0);
    
    // Symmetric Visualizer Logic
    // We calculate half the bars and mirror them
    int half_bars = num_bars / 2;
    
    for (int i = 0; i < half_bars; ++i) {
        if (player.is_playing() && !player.is_paused() && !player.is_idle()) {
            int max_h = draw_h;
            int target = rand() % max_h;
            
            // Smooth transition
            if (bars[i] < target) bars[i] += 1;
            else if (bars[i] > target) bars[i] -= 1;
            
            if (bars[i] < 0) bars[i] = 0;
            if (bars[i] >= draw_h) bars[i] = draw_h - 1;
        } else {
            if (bars[i] > 0) bars[i]--;
        }
        // Mirror
        bars[num_bars - 1 - i] = bars[i];
    }
    
    // Center bar (if odd)
    if (num_bars % 2 != 0) {
        int center = num_bars / 2;
        if (player.is_playing() && !player.is_paused() && !player.is_idle()) {
             int target = rand() % draw_h;
             if (bars[center] < target) bars[center] += 1;
             else if (bars[center] > target) bars[center] -= 1;
        } else {
             if (bars[center] > 0) bars[center]--;
        }
    }
    
    wattron(visualizer_win, COLOR_PAIR(3) | A_BOLD);
    for (int i = 0; i < num_bars; ++i) {
        int bar_height = bars[i];
        
        // Draw from bottom up
        for (int y = 0; y < bar_height; ++y) {
            int draw_y = height - 2 - y;
            for (int k = 0; k < bar_width; ++k) {
                 // Use block characters for cleaner look if terminal supports, 
                 // but for ncurses safety stick to simple chars or ACS
                 // ACS_BLOCK is often solid
                 mvwaddch(visualizer_win, draw_y, (i * bar_width) + 1 + k, ACS_CKBOARD); 
            }
        }
    }
    wattroff(visualizer_win, COLOR_PAIR(3) | A_BOLD);
    
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
        else if (mode == AppMode::LYRICS_VIEW)
             mvwprintw(help_win, 1, 2, "[UP/DOWN] Scroll [ESC] Back");
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
        else if (mode == AppMode::LYRICS_VIEW) handle_lyrics_input(ch);
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

        case KEY_UP: 
            if (lyrics_scroll_offset > 0) lyrics_scroll_offset--; 
            // lyrics_auto_scroll = false; // Always on
            break;
        case KEY_DOWN: 
            lyrics_scroll_offset++; 
            // lyrics_auto_scroll = false; // Always on
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
                player.stop(); // Stop current playback
                fetch_current_lyrics(item.path); // Fetch BEFORE loading/playing
                player.load(item.path);
                last_played_path = item.path;
                player.set_property("force-media-title", item.path); 
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
                    player.stop(); // Stop current playback
                    show_message("Resolving stream...");
                    wrefresh(help_win);
                    std::string stream_url = get_youtube_stream_url(search_results[selection_index].url);
                    
                    fetch_current_lyrics(search_results[selection_index].title); // Fetch BEFORE loading/playing
                    
                    player.load(stream_url);
                    last_played_path = stream_url;
                    player.set_property("force-media-title", search_results[selection_index].title);
                    
                    // Set autoplay context
                    
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

void UI::update_preview_songs() {
    if (playlists.empty() || selection_index < 0 || selection_index >= playlists.size()) {
        preview_songs.clear();
        return;
    }
    preview_songs = playlist_manager.get_playlist_songs(playlists[selection_index].name);
}

void UI::draw_playlists() {
    werase(main_win);
    draw_borders(main_win, "PLAYLISTS");
    
    int height, width;
    getmaxyx(main_win, height, width);
    
    if (playlists.empty()) {
        mvwprintw(main_win, height/2, 2, "No playlists found. Press [N] to create one.");
        wrefresh(main_win);
        return;
    }

    // Split layout: 30% list, 70% preview
    int list_width = width * 0.3;
    int preview_start_x = list_width + 1;
    int preview_width = width - preview_start_x - 2;

    // Draw Separator
    for (int i = 1; i < height - 1; ++i) {
        mvwaddch(main_win, i, list_width, ACS_VLINE);
    }
    mvwaddch(main_win, 0, list_width, ACS_TTEE);
    mvwaddch(main_win, height - 1, list_width, ACS_BTEE);

    // Draw Playlist List (Left Side)
    wattron(main_win, A_BOLD | A_UNDERLINE);
    mvwprintw(main_win, 1, 2, "%-20s", "Playlist Name");
    wattroff(main_win, A_BOLD | A_UNDERLINE);
    
    for (int i = 0; i < playlists.size(); ++i) {
        int y = i + 2;
        if (y >= height - 1) break;
        
        if (i == selection_index) wattron(main_win, COLOR_PAIR(6));
        
        std::string name = playlists[i].name;
        std::string count_str = " (" + std::to_string(playlists[i].song_count) + ")";
        
        int max_name_len = list_width - 4 - count_str.length();
        if (max_name_len < 1) max_name_len = 1;
        
        if (name.length() > max_name_len) {
            if (max_name_len > 3) {
                name = name.substr(0, max_name_len - 3) + "...";
            } else {
                name = name.substr(0, max_name_len);
            }
        }
        
        mvwprintw(main_win, y, 2, "%s%s", name.c_str(), count_str.c_str());
        
        if (i == selection_index) wattroff(main_win, COLOR_PAIR(6));
    }

    // Draw Preview (Right Side)
    if (selection_index >= 0 && selection_index < playlists.size()) {
        std::string preview_title = "Preview: " + playlists[selection_index].name;
        if (preview_title.length() > preview_width) preview_title = preview_title.substr(0, preview_width - 3) + "...";
        
        wattron(main_win, A_BOLD);
        mvwprintw(main_win, 1, preview_start_x + 2, "%s", preview_title.c_str());
        wattroff(main_win, A_BOLD);

        if (preview_songs.empty()) {
            mvwprintw(main_win, 3, preview_start_x + 2, "Playlist is empty.");
        } else {
            int max_title_len = preview_width - 20; // Adjust for index (4+1) and duration (1+10) + padding
            if (max_title_len < 10) max_title_len = 10;

            wattron(main_win, A_UNDERLINE);
            mvwprintw(main_win, 2, preview_start_x + 2, "%-4s %-*s %10s", "#", max_title_len, "Title", "Duration");
            wattroff(main_win, A_UNDERLINE);

            for (int i = 0; i < preview_songs.size(); ++i) {
                int y = i + 3;
                if (y >= height - 1) break;

                std::string title = preview_songs[i].title;
                if (title.length() > max_title_len) title = title.substr(0, max_title_len - 3) + "...";

                mvwprintw(main_win, y, preview_start_x + 2, "%-4d %-*s %10s", 
                          i + 1, max_title_len, title.c_str(), preview_songs[i].duration.c_str());
            }
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
        int max_title_len = width - 20; // Adjust for index (4+1) and duration (1+10) + padding
        if (max_title_len < 10) max_title_len = 10;

        // Header
        wattron(main_win, A_BOLD | A_UNDERLINE);
        mvwprintw(main_win, 1, 2, "%-4s %-*s %10s", "#", max_title_len, "Title", "Duration");
        wattroff(main_win, A_BOLD | A_UNDERLINE);
        
        for (int i = 0; i < current_playlist_songs.size(); ++i) {
            int y = i + 2;
            if (y >= height - 1) break;
            
            if (i == selection_index) wattron(main_win, COLOR_PAIR(6));
            
            std::string title = current_playlist_songs[i].title;
            if (title.length() > max_title_len) title = title.substr(0, max_title_len - 3) + "...";
            
            mvwprintw(main_win, y, 2, "%-4d %-*s %10s", i + 1, max_title_len, title.c_str(), current_playlist_songs[i].duration.c_str());
            
            if (i == selection_index) wattroff(main_win, COLOR_PAIR(6));
        }
    }
    wrefresh(main_win);
}

void UI::handle_playlists_input(int ch) {
    switch (ch) {
        case 27: set_mode(AppMode::PLAYBACK); break;
        case KEY_UP: 
            if (selection_index > 0) {
                selection_index--; 
                update_preview_songs();
            }
            break;
        case KEY_DOWN: 
            if (selection_index < playlists.size() - 1) {
                selection_index++; 
                update_preview_songs();
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
                    update_preview_songs();
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
                update_preview_songs();
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
                    player.stop(); // Stop current playback
                    fetch_current_lyrics(current_playlist_songs[selection_index].title); // Fetch BEFORE loading/playing
                    
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

void UI::draw_lyrics() {
    // Determine target window based on mode
    WINDOW* target_win = (mode == AppMode::LYRICS_VIEW) ? main_win : lyrics_win;
    
    werase(target_win);
    draw_borders(target_win, "LYRICS");
    
    int height, width;
    getmaxyx(target_win, height, width);
    int text_h = height - 2;
    int text_w = width - 4;
    
    if (current_lyrics_data.has_synced) {
        // Synced Lyrics Logic
        double current_time = player.get_position();
        int active_index = -1;
        
        // Find active line
        for (int i = 0; i < current_lyrics_data.synced_lyrics.size(); ++i) {
            if (current_lyrics_data.synced_lyrics[i].timestamp <= current_time) {
                active_index = i;
            } else {
                break;
            }
        }
        
        // Auto-scroll
        if (lyrics_auto_scroll && active_index != -1) {
            // Try to center the active line
            int target_offset = active_index - (text_h / 2);
            if (target_offset < 0) target_offset = 0;
            lyrics_scroll_offset = target_offset;
        }
        
        // Draw lines
        for (int i = 0; i < text_h; ++i) {
            int idx = i + lyrics_scroll_offset;
            if (idx >= current_lyrics_data.synced_lyrics.size()) break;
            
            if (idx == active_index) {
                wattron(target_win, A_BOLD | COLOR_PAIR(2)); // Highlight active line
                std::string line = "> " + current_lyrics_data.synced_lyrics[idx].text;
                int start_x = (width - line.length()) / 2;
                if (start_x < 0) start_x = 0;
                mvwprintw(target_win, i + 1, start_x, "%s", line.c_str());
                wattroff(target_win, A_BOLD | COLOR_PAIR(2));
            } else {
                std::string line = current_lyrics_data.synced_lyrics[idx].text;
                int start_x = (width - line.length()) / 2;
                if (start_x < 0) start_x = 0;
                mvwprintw(target_win, i + 1, start_x, "%s", line.c_str());
            }
        }
        
    } else {
        // Plain Lyrics Logic (Fallback)
        // Check for error messages
        bool is_error = (current_lyrics_data.plain_lyrics.find("not found") != std::string::npos || 
                         current_lyrics_data.plain_lyrics.find("missing") != std::string::npos ||
                         current_lyrics_data.plain_lyrics.find("error") != std::string::npos);
                         
        if (is_error) {
            // Centered Error Display
            std::string error_msg = current_lyrics_data.plain_lyrics;
            if (error_msg.length() > text_w) error_msg = error_msg.substr(0, text_w);
            
            int start_y = height / 2;
            int start_x = (width - error_msg.length()) / 2;
            if (start_x < 0) start_x = 0;
            
            wattron(target_win, COLOR_PAIR(1) | A_BOLD); // Red/Warning color
            mvwprintw(target_win, start_y, start_x, "%s", error_msg.c_str());
            
            // Draw a box around it? Maybe too much. Let's just make it bold red.
            // Add a "Try searching manually?" hint below
            std::string hint = "(Press 'S' to search for another version)";
            int hint_x = (width - hint.length()) / 2;
            if (hint_x < 0) hint_x = 0;
            wattroff(target_win, A_BOLD);
            mvwprintw(target_win, start_y + 2, hint_x, "%s", hint.c_str());
            
            wattroff(target_win, COLOR_PAIR(1));
            
        } else {
            // Normal Plain Lyrics
            std::vector<std::string> lines;
            std::string current_line;
            for (char c : current_lyrics_data.plain_lyrics) {
                if (c == '\n') {
                    lines.push_back(current_line);
                    current_line = "";
                } else {
                    current_line += c;
                }
            }
            lines.push_back(current_line);
            
            // Word wrap (simple)
            std::vector<std::string> wrapped_lines;
            for (const auto& line : lines) {
                if (line.length() <= text_w) {
                    wrapped_lines.push_back(line);
                } else {
                    std::string temp = line;
                    while (temp.length() > text_w) {
                        wrapped_lines.push_back(temp.substr(0, text_w));
                        temp = temp.substr(text_w);
                    }
                    wrapped_lines.push_back(temp);
                }
            }
            
            for (int i = 0; i < text_h && (i + lyrics_scroll_offset) < wrapped_lines.size(); ++i) {
                std::string line = wrapped_lines[i + lyrics_scroll_offset];
                int start_x = (width - line.length()) / 2;
                if (start_x < 0) start_x = 0;
                mvwprintw(target_win, i + 1, start_x, "%s", line.c_str());
            }
        }
    }
    
    wrefresh(target_win);
}

void UI::handle_lyrics_input(int ch) {
    switch (ch) {
        case 27: set_mode(AppMode::PLAYBACK); break;
        case KEY_UP: 
            if (lyrics_scroll_offset > 0) lyrics_scroll_offset--; 
            lyrics_auto_scroll = false;
            break;
        case KEY_DOWN: 
            lyrics_scroll_offset++; 
            lyrics_auto_scroll = false;
            break;
        case 'a': case 'A':
            lyrics_auto_scroll = !lyrics_auto_scroll;
            show_message(std::string("Auto-scroll: ") + (lyrics_auto_scroll ? "ON" : "OFF"));
            break;
    }
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
        
        player.stop(); // Stop current playback
        std::string stream_url = get_youtube_stream_url(next_url);
        fetch_current_lyrics(next_title); // Fetch BEFORE loading/playing
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

void UI::fetch_current_lyrics(std::string title_override) {
    std::string title = title_override;
    if (title.empty()) {
        title = player.get_metadata("media-title");
        if (title.empty()) title = player.get_metadata("filename");
    }
    
    // Remove extension if present (simple check)
    size_t last_dot = title.find_last_of(".");
    if (last_dot != std::string::npos && last_dot > title.length() - 5) {
        title = title.substr(0, last_dot);
    }
    
    std::string artist = "";
    std::string song_title = title;
    
    // 1. Try "Artist - Title" format
    size_t dash_pos = title.find(" - ");
    if (dash_pos != std::string::npos) {
        artist = title.substr(0, dash_pos);
        song_title = title.substr(dash_pos + 3);
    }
    
    // 2. If no artist, try metadata
    if (artist.empty()) {
         artist = player.get_metadata("artist");
    }
    
    // 3. If still no artist, use "Unknown" or just try to search with title if API allows (it usually needs artist)
    // But let's be smarter. If we have no artist, we can't really query the API effectively without one.
    // However, we can try to assume the whole title is the song title and pass a dummy artist or try to extract from title more aggressively?
    // Let's just use "Unknown" for now, but update the error message to be less specific about "Artist - Title".
    
    show_message("Fetching lyrics...");
    wrefresh(help_win); 
    
    if (artist.empty()) {
        // Try to fetch with empty artist? It might fail.
        // Let's try to pass the whole title as song_title and see what happens if we pass " " as artist.
        // Actually, let's just fail gracefully with a better message.
        current_lyrics_data = {"Lyrics not found. Could not detect artist.", {}, false};
    } else {
        current_lyrics_data = lyrics_manager.fetch_lyrics(artist, song_title);
    }
    
    lyrics_scroll_offset = 0;
    lyrics_auto_scroll = true;
}
