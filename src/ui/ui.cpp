#include "ui.hpp"
#include "utils.hpp"
#include "mpris.hpp"
#include "updater.hpp"
#include <ncurses.h>
#include <cmath>
#include <vector>
#include <chrono>
#include <thread>
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

UI::UI(Player& p) 
    : player(p), 
      running(true), 
      mode(AppMode::INTRO), 
      main_win(nullptr),
      visualizer_win(nullptr),
      status_win(nullptr),
      help_win(nullptr),
      lyrics_win(nullptr),
      selection_index(0), 
      scroll_offset(0), 
      current_visualizer_mode(VisualizerMode::CAVA_WAVE), 
      current_theme_idx(0),
      lyrics_scroll_offset(0), 
      lyrics_auto_scroll(true),
      autoplay_enabled(true),
      playing_index(-1),
      is_playing_from_playlist(false),
      current_playback_source(PlaybackSource::NONE),
      queue_index(-1),
      track_retry_count(0),
      song_to_move_index(-1),
      last_key(0) 
{
    discord_rpc = std::make_unique<DiscordRPC>("1345437817082089472"); // Vibe-Fi Client ID

    set_escdelay(25);
    setlocale(LC_ALL, "");
    setlocale(LC_NUMERIC, "C"); // libmpv requires LC_NUMERIC to remain "C"
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    timeout(35); // ~28 FPS for silky-smooth visualizer and responsive controls 
    
    start_color();
    use_default_colors();
    
    load_themes();
    load_saved_settings();
    apply_theme();

    refresh(); // Refresh stdscr before creating windows
    
    // Initialize library root
    current_path = library.get_home_music_dir();
    library_items = library.list_directory(current_path);

    // Start background MPRIS listener (actions polled thread-safely in UI loop)
    start_mpris_server();

    // Start background update check (rate-limited, queries GitHub after 5s)
    start_background_update_check([this](const std::string& version) {
        this->notify_update_available(version);
    });
}

UI::~UI() {
    stop_background_update_check();
    stop_mpris_server();
    save_state();

    if (lyrics_win) delwin(lyrics_win);
    if (visualizer_win) delwin(visualizer_win);
    if (status_win) delwin(status_win);
    if (help_win) delwin(help_win);
    if (main_win) delwin(main_win);
    endwin();
}

WINDOW* UI::create_window(int height, int width, int starty, int startx) {
    return newwin(height, width, starty, startx);
}

void UI::draw_borders(WINDOW* win, const std::string& title) {
    wattron(win, COLOR_PAIR(1));
    box(win, 0, 0);
    if (!title.empty()) {
        mvwprintw(win, 0, 2, " %s ", title.c_str());
    }
    wattroff(win, COLOR_PAIR(1));
    wnoutrefresh(win);
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

void UI::set_initial_queue(const std::vector<SearchResult>& results) {
    play_queue.clear();
    search_results = results;
    for (const auto& res : results) {
        play_queue.push_back({res.title, res.url, res.duration});
    }
    queue_index = 0;
    is_playing_from_playlist = false;
    playing_playlist_name.clear();
    current_playback_source = PlaybackSource::SEARCH;
    if (!play_queue.empty()) {
        last_played_path = play_queue[0].url;
        player.set_property("force-media-title", play_queue[0].title);
        fetch_current_lyrics(play_queue[0].title, play_queue[0].url);
    }
}

void UI::run() {
    int height, width;
    getmaxyx(stdscr, height, width);

    // Layout calculation
    int help_h = 3;
    int status_h = 5;
    int main_h = height - status_h - help_h;
    if (main_h < 6) main_h = 6;
    
    // Split main area: Top 40% for visualizer, Bottom 60% for lyrics
    int viz_h = static_cast<int>(main_h * 0.4);
    if (viz_h < 3) viz_h = 3;
    int lyrics_h = main_h - viz_h;

    visualizer_win = create_window(viz_h, width, 0, 0); 
    lyrics_win     = create_window(lyrics_h, width, viz_h, 0);
    main_win       = create_window(main_h, width, 0, 0);
    status_win     = create_window(status_h, width, main_h, 0);
    help_win       = create_window(help_h, width, main_h + status_h, 0);

    while (running) {
        // Handle window resizing dynamically
        int new_h, new_w;
        getmaxyx(stdscr, new_h, new_w);
        if (new_h != height || new_w != width) {
            height = new_h;
            width = new_w;
            main_h = height - status_h - help_h;
            if (main_h < 6) main_h = 6;
            viz_h = static_cast<int>(main_h * 0.4);
            if (viz_h < 3) viz_h = 3;
            lyrics_h = main_h - viz_h;
            
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

        // Thread-safe processing of MPRIS media key events on the UI thread
        MprisAction mpris_act;
        while (poll_mpris_action(mpris_act)) {
            switch (mpris_act) {
                case MprisAction::PLAY_PAUSE:
                    player.toggle_pause();
                    break;
                case MprisAction::NEXT:
                    play_next();
                    break;
                case MprisAction::PREVIOUS:
                    play_previous();
                    break;
                case MprisAction::STOP:
                    player.stop();
                    break;
                case MprisAction::NONE:
                    break;
            }
        }

        // Check for update notification from background thread
        if (has_pending_update_notification.load()) {
            std::string ver;
            {
                std::lock_guard<std::mutex> lock(update_notification_mutex);
                ver = pending_update_version;
                has_pending_update_notification.store(false);
            }
            if (!ver.empty()) {
                show_message("Update available: " + ver + " (restart or vibe -u)");
            }
        }

        draw();
        handle_input();
        
        // Poll mpv events (EOF, ERROR, FILE_LOADED, etc.)
        player.poll_events();

        // Autoplay check: transition to next track ONLY when track naturally finishes (EOF)
        if (player.consume_track_finished()) {
            track_retry_count = 0;
            if (autoplay_enabled && !play_queue.empty() && queue_index >= 0) {
                play_next();
            }
        } else if (player.consume_playback_error()) {
            // Playback or buffering error occurred
            if (queue_index >= 0 && queue_index < static_cast<int>(play_queue.size())) {
                const auto& song = play_queue[queue_index];
                if (track_retry_count < 2) {
                    track_retry_count++;
                    show_message("Streaming error, retrying... (" + std::to_string(track_retry_count) + "/2)");
                    start_track_playback(song.title, song.url);
                } else {
                    track_retry_count = 0;
                    show_message("Failed to stream: " + song.title + " (press R to retry, N for next)");
                }
            } else {
                track_retry_count = 0;
                show_message("Playback error: " + player.get_last_error());
            }
        }

        // Auto-fetch lyrics as soon as media title metadata is resolved by player
        if (mode == AppMode::PLAYBACK && (player.is_playing() || !player.is_idle())) {
            std::string active_title = player.get_metadata("media-title");
            if (!active_title.empty() && active_title != current_lyrics_title) {
                fetch_current_lyrics(active_title, last_played_path);
            }
        }
    }
}

void UI::draw() {
    switch (mode) {
        case AppMode::PLAYBACK:
            draw_playback();
            break;
        case AppMode::LIBRARY_BROWSER:
            draw_library();
            break;
        case AppMode::SEARCH_INPUT:
            draw_search_input();
            break;
        case AppMode::SEARCH_RESULTS:
            draw_search_results();
            break;
        case AppMode::INTRO:
            draw_intro();
            break;
        case AppMode::PLAYLIST_BROWSER:
            draw_playlists();
            break;
        case AppMode::PLAYLIST_VIEW:
            draw_playlist_view();
            break;
        case AppMode::PLAYLIST_SELECT_FOR_ADD:
        case AppMode::PLAYLIST_SELECT_FOR_MOVE:
            draw_playlist_select_for_add();
            break;
        case AppMode::LYRICS_VIEW:
            draw_lyrics();
            break;
        case AppMode::QUEUE_VIEW:
            draw_queue();
            break;
    }
    
    update_status();
    update_help();
    doupdate();
}

void UI::draw_playback() {
    update_visualizer();
    draw_lyrics();
}

void UI::update_visualizer() {
    visualizer.render(visualizer_win, player, current_visualizer_mode);
    wnoutrefresh(visualizer_win);
}

void UI::draw_playlist_select_for_add() {
    werase(main_win);
    std::string title = (mode == AppMode::PLAYLIST_SELECT_FOR_MOVE) ? "MOVE SONG TO..." : "SELECT PLAYLIST TO ADD TO";
    draw_borders(main_win, title);
    
    int height, width;
    getmaxyx(main_win, height, width);
    
    if (playlists.empty()) {
        mvwprintw(main_win, height / 2, 2, "No playlists found. Press [N] to create one.");
    } else {
        wattron(main_win, A_BOLD | A_UNDERLINE);
        mvwprintw(main_win, 1, 2, "%-20s %10s", "Playlist Name", "Songs");
        wattroff(main_win, A_BOLD | A_UNDERLINE);
        
        int visible_count = height - 4;
        for (int i = 0; i < visible_count && (i + scroll_offset) < static_cast<int>(playlists.size()); ++i) {
            int actual_idx = i + scroll_offset;
            int y = i + 2;
            
            if (actual_idx == selection_index) wattron(main_win, COLOR_PAIR(6));
            
            std::string name = playlists[actual_idx].name;
            if (name.length() > 20) name = name.substr(0, 17) + "...";
            
            mvwprintw(main_win, y, 2, "%-20s %10d", name.c_str(), playlists[actual_idx].song_count);
            
            if (actual_idx == selection_index) wattroff(main_win, COLOR_PAIR(6));
        }
    }
    
    mvwprintw(main_win, height - 2, 2, "Press [N] to create a new playlist | [ESC] to cancel");
    wnoutrefresh(main_win);
}

void UI::draw_library() {
    werase(main_win);
    draw_borders(main_win, "LIBRARY: " + current_path);
    
    int height, width;
    getmaxyx(main_win, height, width);
    int list_h = height - 2;
    
    for (int i = 0; i < list_h && (i + scroll_offset) < static_cast<int>(library_items.size()); ++i) {
        int idx = i + scroll_offset;
        const auto& item = library_items[idx];
        
        if (idx == selection_index) {
            wattron(main_win, COLOR_PAIR(6));
        }
        
        std::string display_name = (item.is_directory ? "[DIR] " : "      ") + item.name;
        if (!item.is_directory && !item.duration.empty()) {
            display_name += " (" + item.duration + ")";
        }
        if (static_cast<int>(display_name.length()) > width - 4) {
            display_name = display_name.substr(0, width - 7) + "...";
        }
        mvwprintw(main_win, i + 1, 2, "%s", display_name.c_str());
        
        if (idx == selection_index) {
            wattroff(main_win, COLOR_PAIR(6));
        }
    }
    wnoutrefresh(main_win);
}

void UI::draw_search_input() {
    werase(main_win);
    draw_borders(main_win, "SEARCH YOUTUBE");
    
    int height, width;
    getmaxyx(main_win, height, width);
    
    std::string prompt = "What do you want to listen to?";
    int prompt_x = (width - static_cast<int>(prompt.length())) / 2;
    if (prompt_x < 2) prompt_x = 2;
    int prompt_y = height / 2 - 2;
    
    wattron(main_win, A_BOLD);
    mvwprintw(main_win, prompt_y, prompt_x, "%s", prompt.c_str());
    wattroff(main_win, A_BOLD);
    
    int box_width = std::min(60, width - 6);
    if (box_width < 10) box_width = 10;
    int box_x = (width - box_width) / 2;
    int box_y = prompt_y + 2;
    
    mvwprintw(main_win, box_y, box_x - 2, "> ");
    
    wattron(main_win, COLOR_PAIR(6));
    mvwhline(main_win, box_y, box_x, ' ', box_width);
    
    std::string display_query = search_query;
    if (static_cast<int>(display_query.length()) > box_width - 2) {
        display_query = display_query.substr(display_query.length() - (box_width - 2));
    }
    mvwprintw(main_win, box_y, box_x, "%s", display_query.c_str());
    if (static_cast<int>(search_query.length()) < box_width) {
        waddch(main_win, '_');
    }
    wattroff(main_win, COLOR_PAIR(6));
    
    wnoutrefresh(main_win);
}

void UI::draw_search_results() {
    werase(main_win);
    draw_borders(main_win, "SEARCH RESULTS");
    
    int height, width;
    getmaxyx(main_win, height, width);
    
    if (search_results.empty()) {
        std::string msg = is_online() ? "No results found or searching..." 
                                      : "Network unavailable: Unable to connect to YouTube.";
        mvwprintw(main_win, height / 2, (width - static_cast<int>(msg.length())) / 2, "%s", msg.c_str());
    } else {
        int title_col_width = width - 20;
        if (title_col_width < 15) title_col_width = 15;

        wattron(main_win, A_BOLD | A_UNDERLINE);
        mvwprintw(main_win, 1, 2, "%-4s %-*s %10s", "#", title_col_width, "Title", "Duration");
        wattroff(main_win, A_BOLD | A_UNDERLINE);
        
        int visible_rows = height - 3;
        for (int i = 0; i < visible_rows && (i + scroll_offset) < static_cast<int>(search_results.size()); ++i) {
            int actual_idx = i + scroll_offset;
            int y = i + 2;
            
            if (actual_idx == selection_index) wattron(main_win, COLOR_PAIR(6));
            
            std::string title = search_results[actual_idx].title;
            if (static_cast<int>(title.length()) > title_col_width) {
                title = title.substr(0, title_col_width - 3) + "...";
            }
            
            mvwprintw(main_win, y, 2, "%-4d %-*s %10s", actual_idx + 1, title_col_width, title.c_str(), search_results[actual_idx].duration.c_str());
            
            if (actual_idx == selection_index) wattroff(main_win, COLOR_PAIR(6));
        }
    }
    wnoutrefresh(main_win);
}

void UI::update_status() {
    werase(status_win);
    draw_borders(status_win, "NOW PLAYING");
    
    int height, width;
    getmaxyx(status_win, height, width);
    (void)height;
    
    std::string title = player.get_metadata("media-title");
    if (title.empty()) {
        title = player.get_metadata("filename");
        if (title.empty()) {
            title = player.is_playing() ? "Playing Audio Stream" : "Not Playing";
        }
    }
    
    if (static_cast<int>(title.length()) > width - 4) {
        title = title.substr(0, width - 7) + "...";
    }
    int title_x = (width - static_cast<int>(title.length())) / 2;
    if (title_x < 2) title_x = 2;
    
    wattron(status_win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(status_win, 1, title_x, "%s", title.c_str());
    wattroff(status_win, COLOR_PAIR(1) | A_BOLD);
    
    double pos = player.get_position();
    double dur = player.get_duration();
    int bar_width = width - 4;
    
    if (dur > 0 && bar_width > 4) {
        int filled = static_cast<int>((pos / dur) * (bar_width - 2));
        if (filled > bar_width - 2) filled = bar_width - 2;
        if (filled < 0) filled = 0;

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
    mvwprintw(status_win, 3, width - static_cast<int>(vol_str.length()) - 2, "%s", vol_str.c_str());
    wnoutrefresh(status_win);
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
            mvwprintw(help_win, 1, 2, "[SPACE] Pause [N/B] Next/Prev [Q] Queue [L] Library [S] Search [P] Playlist [R] Replay [O] Autoplay:%s [ESC] Quit", auto_str.c_str());
        } else if (mode == AppMode::LIBRARY_BROWSER) {
            mvwprintw(help_win, 1, 2, "[ENTER] Select/Play [BKSP] Parent Directory [ESC] Playback");
        } else if (mode == AppMode::SEARCH_INPUT) {
            mvwprintw(help_win, 1, 2, "[ENTER] Search YouTube [ESC] Cancel");
        } else if (mode == AppMode::SEARCH_RESULTS) {
            mvwprintw(help_win, 1, 2, "[ENTER] Play [A] Add to Playlist [S] New Search [ESC] Back");
        } else if (mode == AppMode::PLAYLIST_BROWSER) {
            mvwprintw(help_win, 1, 2, "[ENTER] View [N] New [D] Delete [R] Rename [E] Export M3U [ESC] Back");
        } else if (mode == AppMode::PLAYLIST_VIEW) {
            mvwprintw(help_win, 1, 2, "[ENTER] Play [D] Remove Song [M] Move Song [ESC] Back");
        } else if (mode == AppMode::PLAYLIST_SELECT_FOR_ADD || mode == AppMode::PLAYLIST_SELECT_FOR_MOVE) {
            mvwprintw(help_win, 1, 2, "[ENTER] Select [N] New Playlist [ESC] Cancel");
        } else if (mode == AppMode::QUEUE_VIEW) {
            mvwprintw(help_win, 1, 2, "[ENTER] Play Selected [D] Remove [ESC] Back");
        } else if (mode == AppMode::LYRICS_VIEW) {
            mvwprintw(help_win, 1, 2, "[UP/DOWN] Scroll Lyrics [ESC] Back");
        } else if (mode == AppMode::INTRO) {
            mvwprintw(help_win, 1, 2, "[L] Library [S] Search [P] Playlists [R] Resume [ENTER] Library [ESC] Quit");
        }
        wattroff(help_win, COLOR_PAIR(4));
    }
    wnoutrefresh(help_win);
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
        else if (mode == AppMode::PLAYLIST_SELECT_FOR_MOVE) handle_playlist_select_for_move_input(ch);
        else if (mode == AppMode::LYRICS_VIEW) handle_lyrics_input(ch);
        else if (mode == AppMode::QUEUE_VIEW) handle_queue_input(ch);
        else if (mode == AppMode::INTRO) handle_intro_input(ch);
    } catch (const std::exception& e) {
        show_message(std::string("Error: ") + e.what());
    }

    last_key = ch;
}

void UI::handle_playback_input(int ch) {
    switch (ch) {
        case 27: 
            if (confirm_quit()) {
                running = false; 
            }
            break;
        case ' ': 
            player.toggle_pause(); 
            break;
        case 'l': case 'L':
            set_mode(AppMode::LIBRARY_BROWSER); 
            break;
        case 's': case 'S': 
            search_query = ""; 
            set_mode(AppMode::SEARCH_INPUT); 
            break;
        case 'q': case 'Q':
            if (current_playback_source == PlaybackSource::SEARCH && !search_results.empty()) {
                selection_index = (queue_index >= 0 && queue_index < static_cast<int>(search_results.size())) ? queue_index : 0;
                scroll_offset = std::max(0, selection_index - 5);
                set_mode(AppMode::SEARCH_RESULTS);
            } else if (current_playback_source == PlaybackSource::PLAYLIST && !playing_playlist_name.empty()) {
                current_playlist_name = playing_playlist_name;
                current_playlist_songs = playlist_manager.get_playlist_songs(current_playlist_name);
                selection_index = (queue_index >= 0 && queue_index < static_cast<int>(current_playlist_songs.size())) ? queue_index : 0;
                scroll_offset = std::max(0, selection_index - 5);
                set_mode(AppMode::PLAYLIST_VIEW);
            } else if (current_playback_source == PlaybackSource::LIBRARY && !library_items.empty()) {
                set_mode(AppMode::LIBRARY_BROWSER);
            } else if (!search_results.empty()) {
                selection_index = (queue_index >= 0 && queue_index < static_cast<int>(search_results.size())) ? queue_index : 0;
                scroll_offset = std::max(0, selection_index - 5);
                set_mode(AppMode::SEARCH_RESULTS);
            } else if (!playing_playlist_name.empty()) {
                current_playlist_name = playing_playlist_name;
                current_playlist_songs = playlist_manager.get_playlist_songs(current_playlist_name);
                selection_index = (queue_index >= 0 && queue_index < static_cast<int>(current_playlist_songs.size())) ? queue_index : 0;
                scroll_offset = std::max(0, selection_index - 5);
                set_mode(AppMode::PLAYLIST_VIEW);
            } else if (!play_queue.empty()) {
                selection_index = (queue_index >= 0 && queue_index < static_cast<int>(play_queue.size())) ? queue_index : 0;
                scroll_offset = std::max(0, selection_index - 5);
                set_mode(AppMode::QUEUE_VIEW);
            } else {
                show_message("Queue is empty.");
            }
            break;
        case 'r': case 'R': 
            if (!last_played_path.empty()) {
                if (is_url(last_played_path) && !is_online()) {
                    show_message("Network unavailable. Cannot stream track.");
                    break;
                }
                player.load(last_played_path);
                player.play();
                show_message("Replaying...");
            }
            break;
        case KEY_LEFT: 
            player.seek(-5.0); 
            break;
        case KEY_RIGHT: 
            player.seek(5.0); 
            break;
        case '+': case '=': 
            player.set_volume(player.get_volume() + 5); 
            break;
        case '-': case '_': 
            player.set_volume(player.get_volume() - 5); 
            break;
        case 'o': case 'O': 
            autoplay_enabled = !autoplay_enabled; 
            show_message(std::string("Autoplay: ") + (autoplay_enabled ? "ON" : "OFF"));
            break;
        case 'u': case 'U': {
            std::string url = get_user_input("Paste YouTube URL");
            if (!url.empty()) {
                if (!is_online()) {
                    show_message("Network unavailable. Cannot stream online URL.");
                    break;
                }
                show_message("Loading URL...");
                wnoutrefresh(help_win); 
                doupdate();
                std::string stream_url = get_youtube_stream_url(url);
                if (!stream_url.empty()) {
                    player.stop();
                    fetch_current_lyrics("Unknown", url);
                    player.load(stream_url);
                    last_played_path = stream_url;
                    player.set_property("force-media-title", url);
                    is_playing_from_playlist = false;
                    playing_playlist_name.clear();
                    current_playback_source = PlaybackSource::NONE;
                    player.play();
                } else {
                    show_message("Failed to load stream URL.");
                }
            }
            break;
        }
        case 'p': case 'P':
            playlists = playlist_manager.list_playlists();
            set_mode(AppMode::PLAYLIST_BROWSER);
            break;
        case 'c': case 'C':
            selection_index = (queue_index >= 0) ? queue_index : 0;
            scroll_offset = 0;
            set_mode(AppMode::QUEUE_VIEW);
            break;
        case 't': case 'T':
            cycle_theme();
            break;
        case 'v': case 'V':
            cycle_visualizer();
            break;
        case '>': case '.': case 'n': case 'N':
            play_next();
            break;
        case '<': case ',': case 'b': case 'B':
            play_previous();
            break;
        case KEY_UP: 
            if (lyrics_scroll_offset > 0) lyrics_scroll_offset--; 
            break;
        case KEY_DOWN: 
            lyrics_scroll_offset++; 
            break;
    }
}

void UI::handle_library_input(int ch) {
    int height, width;
    getmaxyx(main_win, height, width);
    (void)width;
    int list_h = height - 2;

    switch (ch) {
        case 27: 
            set_mode(AppMode::PLAYBACK); 
            break; 
        case 'k': case KEY_UP: 
            if (selection_index > 0) {
                selection_index--;
                if (selection_index < scroll_offset) {
                    scroll_offset = selection_index;
                }
            }
            break;
        case 'j': case KEY_DOWN:
            if (selection_index < static_cast<int>(library_items.size()) - 1) {
                selection_index++;
                if (list_h > 0 && selection_index >= scroll_offset + list_h) {
                    scroll_offset = selection_index - list_h + 1;
                }
            }
            break;
        case 'h':
        case KEY_BACKSPACE:
        case 127:
            if (current_path != "/") {
                current_path = fs::path(current_path).parent_path().string();
                library_items = library.list_directory(current_path);
                selection_index = 0; 
                scroll_offset = 0;
            }
            break;
        case 'l':
        case 10: // Enter
            if (!library_items.empty()) {
                auto selected_item = library_items[selection_index];
                if (selected_item.is_directory) {
                    current_path = selected_item.path;
                    library_items = library.list_directory(current_path);
                    selection_index = 0; 
                    scroll_offset = 0;
                } else {
                    player.stop();
                    fetch_current_lyrics(selected_item.name, selected_item.path);
                    player.load(selected_item.path);
                    last_played_path = selected_item.path;
                    player.set_property("force-media-title", selected_item.name);
                    
                    play_queue.clear();
                    for (const auto& item : library_items) {
                        if (!item.is_directory) {
                            play_queue.push_back({item.name, item.path, item.duration});
                        }
                    }
                    
                    // Match selection to queue index
                    queue_index = 0;
                    for (size_t i = 0; i < play_queue.size(); ++i) {
                        if (play_queue[i].url == selected_item.path) {
                            queue_index = static_cast<int>(i);
                            break;
                        }
                    }
                    
                    is_playing_from_playlist = false;
                    playing_playlist_name.clear();
                    current_playback_source = PlaybackSource::LIBRARY;
                    player.play();
                    set_mode(AppMode::PLAYBACK);
                }
            }
            break;
        case 'a': case 'A':
            if (!library_items.empty() && !library_items[selection_index].is_directory) {
                song_to_add.title = library_items[selection_index].name;
                song_to_add.url = library_items[selection_index].path;
                song_to_add.duration = library_items[selection_index].duration;
                
                playlists = playlist_manager.list_playlists();
                selection_index = 0;
                scroll_offset = 0;
                set_mode(AppMode::PLAYLIST_SELECT_FOR_ADD);
            }
            break;
    }
}

void UI::handle_search_input_input(int ch) {
    if (ch == 27) { // ESC
        set_mode(AppMode::PLAYBACK);
    } else if (ch == 10) { // Enter
        if (!search_query.empty()) {
            if (!is_online()) {
                show_message("Network unavailable. Please check your connection.");
                return;
            }
            set_mode(AppMode::SEARCH_RESULTS);
            draw();
            show_message("Searching YouTube...");
            wnoutrefresh(help_win);
            doupdate();
            
            search_results = search_youtube(search_query);
            selection_index = 0;
            scroll_offset = 0;
            if (search_results.empty()) {
                if (!is_online()) {
                    show_message("Network unavailable: Unable to reach YouTube.");
                } else {
                    show_message("No results found for: " + search_query);
                }
            }
            draw();
        }
    } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
        if (!search_query.empty()) {
            search_query.pop_back();
        }
    } else if (isprint(ch)) {
        if (search_query.length() < 120) {
            search_query += static_cast<char>(ch);
        }
    }
}

void UI::handle_search_results_input(int ch) {
    int height, width;
    getmaxyx(main_win, height, width);
    (void)width;
    int list_h = height - 3;

    switch (ch) {
        case 27: 
            set_mode(AppMode::PLAYBACK); 
            break;
        case 's': case 'S':
            search_query = "";
            selection_index = 0;
            scroll_offset = 0;
            set_mode(AppMode::SEARCH_INPUT);
            break;
        case 'k': case KEY_UP: 
            if (selection_index > 0) {
                selection_index--;
                if (selection_index < scroll_offset) {
                    scroll_offset = selection_index;
                }
            }
            break;
        case 'j': case KEY_DOWN: 
            if (selection_index < static_cast<int>(search_results.size()) - 1) {
                selection_index++;
                if (list_h > 0 && selection_index >= scroll_offset + list_h) {
                    scroll_offset = selection_index - list_h + 1;
                }
            }
            break;
        case 'G': 
            if (!search_results.empty()) {
                selection_index = static_cast<int>(search_results.size()) - 1;
                if (list_h > 0 && selection_index >= list_h) {
                    scroll_offset = selection_index - list_h + 1;
                }
            }
            break;
        case 'g': 
            if (last_key == 'g') {
                selection_index = 0;
                scroll_offset = 0;
            }
            break;
        case 10: // Enter
            if (!search_results.empty() && selection_index < static_cast<int>(search_results.size())) {
                if (!is_online()) {
                    show_message("Network unavailable. Cannot stream track.");
                    break;
                }
                show_message("Streaming track...");
                wnoutrefresh(help_win);
                doupdate(); 
                
                play_queue.clear();
                for (const auto& res : search_results) {
                    play_queue.push_back({res.title, res.url, res.duration});
                }
                queue_index = selection_index;
                track_retry_count = 0;
                is_playing_from_playlist = false;
                playing_playlist_name.clear();
                current_playback_source = PlaybackSource::SEARCH;
                
                start_track_playback(search_results[selection_index].title, search_results[selection_index].url);
                set_mode(AppMode::PLAYBACK);
            }
            break;
        case 'a': case 'A':
            if (!search_results.empty() && selection_index < static_cast<int>(search_results.size())) {
                song_to_add.title = search_results[selection_index].title;
                song_to_add.url = search_results[selection_index].url;
                song_to_add.duration = search_results[selection_index].duration;
                
                playlists = playlist_manager.list_playlists();
                selection_index = 0;
                scroll_offset = 0;
                set_mode(AppMode::PLAYLIST_SELECT_FOR_ADD);
            }
            break;
    }
}

void UI::update_preview_songs() {
    if (playlists.empty() || selection_index < 0 || selection_index >= static_cast<int>(playlists.size())) {
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
        mvwprintw(main_win, height / 2, 2, "No playlists found. Press [N] to create one.");
        wnoutrefresh(main_win);
        return;
    }

    int list_width = width * 0.35;
    if (list_width < 25) list_width = 25;
    int preview_start_x = list_width + 1;
    int preview_width = width - preview_start_x - 2;

    // Draw vertical separator
    for (int i = 1; i < height - 1; ++i) {
        mvwaddch(main_win, i, list_width, ACS_VLINE);
    }
    mvwaddch(main_win, 0, list_width, ACS_TTEE);
    mvwaddch(main_win, height - 1, list_width, ACS_BTEE);

    // Draw Playlist Names (Left Side)
    wattron(main_win, A_BOLD | A_UNDERLINE);
    mvwprintw(main_win, 1, 2, "%-20s", "Playlist Name");
    wattroff(main_win, A_BOLD | A_UNDERLINE);
    
    int visible_playlists = height - 3;
    for (int i = 0; i < visible_playlists && (i + scroll_offset) < static_cast<int>(playlists.size()); ++i) {
        int actual_idx = i + scroll_offset;
        int y = i + 2;
        
        if (actual_idx == selection_index) wattron(main_win, COLOR_PAIR(6));
        
        std::string name = playlists[actual_idx].name;
        std::string count_str = " (" + std::to_string(playlists[actual_idx].song_count) + ")";
        
        int max_name_len = list_width - 4 - static_cast<int>(count_str.length());
        if (max_name_len < 1) max_name_len = 1;
        
        if (static_cast<int>(name.length()) > max_name_len) {
            name = name.substr(0, std::max(1, max_name_len - 3)) + "...";
        }
        
        mvwprintw(main_win, y, 2, "%s%s", name.c_str(), count_str.c_str());
        
        if (actual_idx == selection_index) wattroff(main_win, COLOR_PAIR(6));
    }

    // Draw Preview (Right Side)
    if (selection_index >= 0 && selection_index < static_cast<int>(playlists.size())) {
        std::string preview_title = "Preview: " + playlists[selection_index].name;
        if (static_cast<int>(preview_title.length()) > preview_width) {
            preview_title = preview_title.substr(0, preview_width - 3) + "...";
        }
        
        wattron(main_win, A_BOLD);
        mvwprintw(main_win, 1, preview_start_x + 2, "%s", preview_title.c_str());
        wattroff(main_win, A_BOLD);

        if (preview_songs.empty()) {
            mvwprintw(main_win, 3, preview_start_x + 2, "Playlist is empty.");
        } else {
            int max_title_len = preview_width - 20;
            if (max_title_len < 10) max_title_len = 10;

            wattron(main_win, A_UNDERLINE);
            mvwprintw(main_win, 2, preview_start_x + 2, "%-4s %-*s %10s", "#", max_title_len, "Title", "Duration");
            wattroff(main_win, A_UNDERLINE);

            int preview_visible = height - 4;
            for (int i = 0; i < preview_visible && i < static_cast<int>(preview_songs.size()); ++i) {
                int y = i + 3;
                std::string title = preview_songs[i].title;
                if (static_cast<int>(title.length()) > max_title_len) {
                    title = title.substr(0, max_title_len - 3) + "...";
                }

                mvwprintw(main_win, y, preview_start_x + 2, "%-4d %-*s %10s", 
                          i + 1, max_title_len, title.c_str(), preview_songs[i].duration.c_str());
            }
        }
    }

    wnoutrefresh(main_win);
}

void UI::draw_playlist_view() {
    werase(main_win);
    draw_borders(main_win, "PLAYLIST: " + current_playlist_name);
    
    int height, width;
    getmaxyx(main_win, height, width);
    
    if (current_playlist_songs.empty()) {
        mvwprintw(main_win, height / 2, 2, "Playlist is empty.");
    } else {
        int max_title_len = width - 20;
        if (max_title_len < 10) max_title_len = 10;

        wattron(main_win, A_BOLD | A_UNDERLINE);
        mvwprintw(main_win, 1, 2, "%-4s %-*s %10s", "#", max_title_len, "Title", "Duration");
        wattroff(main_win, A_BOLD | A_UNDERLINE);
        
        int visible_rows = height - 3;
        for (int i = 0; i < visible_rows && (i + scroll_offset) < static_cast<int>(current_playlist_songs.size()); ++i) {
            int actual_idx = i + scroll_offset;
            int y = i + 2;
            
            if (actual_idx == selection_index) wattron(main_win, COLOR_PAIR(6));
            
            std::string title = current_playlist_songs[actual_idx].title;
            if (static_cast<int>(title.length()) > max_title_len) {
                title = title.substr(0, max_title_len - 3) + "...";
            }
            
            mvwprintw(main_win, y, 2, "%-4d %-*s %10s", actual_idx + 1, max_title_len, title.c_str(), current_playlist_songs[actual_idx].duration.c_str());
            
            if (actual_idx == selection_index) wattroff(main_win, COLOR_PAIR(6));
        }
    }
    wnoutrefresh(main_win);
}

void UI::handle_playlists_input(int ch) {
    int height, width;
    getmaxyx(main_win, height, width);
    (void)width;
    int list_h = height - 3;

    switch (ch) {
        case 27: 
            set_mode(AppMode::PLAYBACK); 
            break;
        case 'k': case KEY_UP: 
            if (selection_index > 0) {
                selection_index--; 
                if (selection_index < scroll_offset) {
                    scroll_offset = selection_index;
                }
                update_preview_songs();
            }
            break;
        case 'j': case KEY_DOWN: 
            if (selection_index < static_cast<int>(playlists.size()) - 1) {
                selection_index++; 
                if (list_h > 0 && selection_index >= scroll_offset + list_h) {
                    scroll_offset = selection_index - list_h + 1;
                }
                update_preview_songs();
            }
            break;
        case 'n': case 'N': {
            std::string name = get_user_input("New Playlist Name");
            if (!name.empty()) {
                if (playlist_manager.create_playlist(name)) {
                    playlists = playlist_manager.list_playlists();
                    show_message("Playlist created.");
                    selection_index = static_cast<int>(playlists.size()) - 1;
                    update_preview_songs();
                } else {
                    show_message("Playlist already exists.");
                }
            }
            break;
        }
        case 'd': case 'D':
            if (!playlists.empty() && selection_index < static_cast<int>(playlists.size())) {
                playlist_manager.delete_playlist(playlists[selection_index].name);
                playlists = playlist_manager.list_playlists();
                if (selection_index >= static_cast<int>(playlists.size()) && selection_index > 0) {
                    selection_index--;
                }
                update_preview_songs();
                show_message("Playlist deleted.");
            }
            break;
        case 'r': case 'R':
            if (!playlists.empty() && selection_index < static_cast<int>(playlists.size())) {
                std::string old_name = playlists[selection_index].name;
                std::string new_name = get_user_input("Rename to");
                if (!new_name.empty()) {
                    if (playlist_manager.rename_playlist(old_name, new_name)) {
                        playlists = playlist_manager.list_playlists();
                        update_preview_songs();
                        show_message("Playlist renamed.");
                    } else {
                        show_message("Rename failed (name already exists?).");
                    }
                }
            }
            break;
        case 'e': case 'E':
            if (!playlists.empty() && selection_index < static_cast<int>(playlists.size())) {
                std::string out_path = get_user_input("Export M3U Path (e.g. /path/to/playlist.m3u)");
                if (!out_path.empty()) {
                    if (playlist_manager.export_to_m3u(playlists[selection_index].name, out_path)) {
                        show_message("Exported successfully.");
                    } else {
                        show_message("Export failed.");
                    }
                }
            }
            break;
        case 10: // Enter
            if (!playlists.empty() && selection_index < static_cast<int>(playlists.size())) {
                current_playlist_name = playlists[selection_index].name;
                current_playlist_songs = playlist_manager.get_playlist_songs(current_playlist_name);
                selection_index = 0;
                scroll_offset = 0;
                set_mode(AppMode::PLAYLIST_VIEW);
            }
            break;
    }
}

void UI::handle_playlist_view_input(int ch) {
    int height, width;
    getmaxyx(main_win, height, width);
    (void)width;
    int list_h = height - 3;

    switch (ch) {
        case 27: 
            playlists = playlist_manager.list_playlists();
            set_mode(AppMode::PLAYLIST_BROWSER); 
            break;
        case 'k': case KEY_UP: 
            if (selection_index > 0) {
                selection_index--; 
                if (selection_index < scroll_offset) {
                    scroll_offset = selection_index;
                }
            }
            break;
        case 'j': case KEY_DOWN: 
            if (selection_index < static_cast<int>(current_playlist_songs.size()) - 1) {
                selection_index++; 
                if (list_h > 0 && selection_index >= scroll_offset + list_h) {
                    scroll_offset = selection_index - list_h + 1;
                }
            }
            break;
        case 'd': case 'D':
            if (!current_playlist_songs.empty() && selection_index < static_cast<int>(current_playlist_songs.size())) {
                playlist_manager.remove_song_from_playlist(current_playlist_name, selection_index);
                current_playlist_songs = playlist_manager.get_playlist_songs(current_playlist_name);
                if (selection_index >= static_cast<int>(current_playlist_songs.size()) && selection_index > 0) {
                    selection_index--;
                }
                show_message("Song removed from playlist.");
            }
            break;
        case 'm': case 'M':
            if (!current_playlist_songs.empty() && selection_index < static_cast<int>(current_playlist_songs.size())) {
                song_to_move_index = selection_index;
                song_to_move_origin_playlist = current_playlist_name;
                playlists = playlist_manager.list_playlists();
                selection_index = 0;
                scroll_offset = 0;
                set_mode(AppMode::PLAYLIST_SELECT_FOR_MOVE);
            }
            break;
        case 10: // Enter
            if (!current_playlist_songs.empty() && selection_index < static_cast<int>(current_playlist_songs.size())) {
                auto song = current_playlist_songs[selection_index];
                if (is_url(song.url) && !is_online()) {
                    show_message("Network unavailable. Cannot play online track.");
                    break;
                }
                show_message("Playing: " + song.title);
                wnoutrefresh(help_win);
                doupdate();

                play_queue = current_playlist_songs;
                queue_index = selection_index;
                track_retry_count = 0;
                playing_playlist_name = current_playlist_name;
                is_playing_from_playlist = true;
                current_playback_source = PlaybackSource::PLAYLIST;
                
                start_track_playback(song.title, song.url);
                set_mode(AppMode::PLAYBACK);
            }
            break;
    }
}

void UI::handle_playlist_select_for_add_input(int ch) {
    int height, width;
    getmaxyx(main_win, height, width);
    (void)width;
    int list_h = height - 4;

    switch (ch) {
        case 27: 
            set_mode(AppMode::PLAYBACK); 
            break;
        case 'k': case KEY_UP: 
            if (selection_index > 0) {
                selection_index--;
                if (selection_index < scroll_offset) {
                    scroll_offset = selection_index;
                }
            }
            break;
        case 'j': case KEY_DOWN: 
            if (selection_index < static_cast<int>(playlists.size()) - 1) {
                selection_index++;
                if (list_h > 0 && selection_index >= scroll_offset + list_h) {
                    scroll_offset = selection_index - list_h + 1;
                }
            }
            break;
        case 10: // Enter
            if (!playlists.empty() && selection_index < static_cast<int>(playlists.size())) {
                std::string target_playlist = playlists[selection_index].name;
                if (playlist_manager.add_song_to_playlist(target_playlist, song_to_add)) {
                    show_message("Added to " + target_playlist);
                    set_mode(AppMode::PLAYBACK);
                } else {
                    show_message("Song already in " + target_playlist);
                }
            }
            break;
        case 'n': case 'N': {
            std::string name = get_user_input("New Playlist Name");
            if (!name.empty()) {
                if (playlist_manager.create_playlist(name)) {
                    playlists = playlist_manager.list_playlists();
                    show_message("Playlist created.");
                    selection_index = static_cast<int>(playlists.size()) - 1;
                } else {
                    show_message("Playlist already exists.");
                }
            }
            break;
        }
    }
}

void UI::handle_playlist_select_for_move_input(int ch) {
    int height, width;
    getmaxyx(main_win, height, width);
    (void)width;
    int list_h = height - 4;

    switch (ch) {
        case 27: 
            set_mode(AppMode::PLAYLIST_VIEW); 
            break;
        case 'k': case KEY_UP: 
            if (selection_index > 0) {
                selection_index--;
                if (selection_index < scroll_offset) {
                    scroll_offset = selection_index;
                }
            }
            break;
        case 'j': case KEY_DOWN: 
            if (selection_index < static_cast<int>(playlists.size()) - 1) {
                selection_index++;
                if (list_h > 0 && selection_index >= scroll_offset + list_h) {
                    scroll_offset = selection_index - list_h + 1;
                }
            }
            break;
        case 10: // Enter
            if (!playlists.empty() && selection_index < static_cast<int>(playlists.size())) {
                std::string target_playlist = playlists[selection_index].name;
                if (target_playlist == song_to_move_origin_playlist) {
                    show_message("Target is same as origin.");
                } else {
                    if (playlist_manager.move_song(song_to_move_origin_playlist, song_to_move_index, target_playlist)) {
                        show_message("Song moved to " + target_playlist);
                        current_playlist_songs = playlist_manager.get_playlist_songs(current_playlist_name);
                        set_mode(AppMode::PLAYLIST_VIEW);
                    } else {
                        show_message("Move failed (already exists in destination?).");
                    }
                }
            }
            break;
        case 'n': case 'N': {
            std::string name = get_user_input("New Playlist Name");
            if (!name.empty()) {
                if (playlist_manager.create_playlist(name)) {
                    playlists = playlist_manager.list_playlists();
                    show_message("Playlist created.");
                    selection_index = static_cast<int>(playlists.size()) - 1;
                } else {
                    show_message("Playlist already exists.");
                }
            }
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
    
    int start_y = (height - static_cast<int>(ascii_art.size())) / 2 - 3;
    if (start_y < 1) start_y = 1;

    wattron(main_win, COLOR_PAIR(1) | A_BOLD);
    for (size_t i = 0; i < ascii_art.size(); ++i) {
        int start_x = (width - static_cast<int>(ascii_art[i].length())) / 2;
        if (start_x < 2) start_x = 2;
        mvwprintw(main_win, start_y + static_cast<int>(i), start_x, "%s", ascii_art[i].c_str());
    }
    wattroff(main_win, COLOR_PAIR(1) | A_BOLD);
    
#ifndef VIBE_FI_VERSION
#define VIBE_FI_VERSION "1.1.1"
#endif
    std::string welcome = std::string("Vibe-Fi Terminal Music Player (v") + VIBE_FI_VERSION + ")";
    int welcome_x = (width - static_cast<int>(welcome.length())) / 2;
    if (welcome_x < 2) welcome_x = 2;
    mvwprintw(main_win, start_y + static_cast<int>(ascii_art.size()) + 2, welcome_x, "%s", welcome.c_str());
    
    std::string instruction = "[L] Library   [S] Search   [P] Playlists   [R] Resume   [ESC] Quit";
    int inst_x = (width - static_cast<int>(instruction.length())) / 2;
    if (inst_x < 2) inst_x = 2;
    mvwprintw(main_win, start_y + static_cast<int>(ascii_art.size()) + 4, inst_x, "%s", instruction.c_str());
    
    wnoutrefresh(main_win);
}

void UI::handle_intro_input(int ch) {
    if (ch == 10 || ch == 'l' || ch == 'L') {
        current_path = library.get_home_music_dir();
        library_items = library.list_directory(current_path);
        set_mode(AppMode::LIBRARY_BROWSER);
    } else if (ch == 's' || ch == 'S') {
        search_query = "";
        set_mode(AppMode::SEARCH_INPUT);
    } else if (ch == 'p' || ch == 'P') {
        playlists = playlist_manager.list_playlists();
        set_mode(AppMode::PLAYLIST_BROWSER);
    } else if (ch == 'r' || ch == 'R') {
        load_state();
    } else if (ch == 27 || ch == 'q' || ch == 'Q') {
        running = false;
    }
}

void UI::draw_lyrics() {
    WINDOW* target_win = (mode == AppMode::LYRICS_VIEW) ? main_win : lyrics_win;
    werase(target_win);
    draw_borders(target_win, "LYRICS");
    
    int height, width;
    getmaxyx(target_win, height, width);
    int text_h = height - 2;
    int text_w = width - 4;
    if (text_h <= 0 || text_w <= 0) {
        wnoutrefresh(target_win);
        return;
    }
    
    int lyrics_w = text_w;
    int lyrics_start = 2;

    if (current_lyrics_data.has_synced) {
        double current_time = player.get_position();
        int active_index = -1;
        for (size_t i = 0; i < current_lyrics_data.synced_lyrics.size(); ++i) {
            if (current_lyrics_data.synced_lyrics[i].timestamp <= current_time) {
                active_index = static_cast<int>(i);
            } else {
                break;
            }
        }
        
        if (lyrics_auto_scroll && active_index != -1) {
            int target_offset = active_index - (text_h / 2);
            if (target_offset < 0) target_offset = 0;
            lyrics_scroll_offset = target_offset;
        }
        
        for (int i = 0; i < text_h; ++i) {
            int idx = i + lyrics_scroll_offset;
            if (idx >= static_cast<int>(current_lyrics_data.synced_lyrics.size())) break;
            
            std::string line = current_lyrics_data.synced_lyrics[idx].text;
            if (idx == active_index) {
                line = "> " + line;
            }

            int line_len = static_cast<int>(line.length());
            if (line_len > lyrics_w) {
                line = line.substr(0, lyrics_w);
                line_len = lyrics_w;
            }

            int start_x = lyrics_start + std::max(0, (lyrics_w - line_len) / 2);

            if (idx == active_index) {
                wattron(target_win, A_BOLD | COLOR_PAIR(2));
                mvwprintw(target_win, i + 1, start_x, "%s", line.c_str());
                wattroff(target_win, A_BOLD | COLOR_PAIR(2));
            } else {
                mvwprintw(target_win, i + 1, start_x, "%s", line.c_str());
            }
        }
    } else {
        bool is_error = (current_lyrics_data.plain_lyrics.find("not found") != std::string::npos || 
                         current_lyrics_data.plain_lyrics.find("missing") != std::string::npos ||
                         current_lyrics_data.plain_lyrics.find("error") != std::string::npos);
        if (is_error) {
            std::string error_msg = current_lyrics_data.plain_lyrics;
            if (static_cast<int>(error_msg.length()) > lyrics_w) {
                error_msg = error_msg.substr(0, lyrics_w);
            }
            int start_y = height / 2;
            int start_x = lyrics_start + std::max(0, (lyrics_w - static_cast<int>(error_msg.length())) / 2);

            wattron(target_win, COLOR_PAIR(1) | A_BOLD);
            mvwprintw(target_win, start_y, start_x, "%s", error_msg.c_str());
            
            std::string hint = "(Press 'S' to search YouTube)";
            int hint_x = lyrics_start + std::max(0, (lyrics_w - static_cast<int>(hint.length())) / 2);
            wattroff(target_win, A_BOLD);
            mvwprintw(target_win, start_y + 2, hint_x, "%s", hint.c_str());
            wattroff(target_win, COLOR_PAIR(1));
        } else {
            std::vector<std::string> lines;
            std::string current_line;
            for (char c : current_lyrics_data.plain_lyrics) {
                if (c == '\n') {
                    lines.push_back(current_line);
                    current_line.clear();
                } else if (c != '\r') {
                    current_line += c;
                }
            }
            lines.push_back(current_line);

            for (int i = 0; i < text_h && (i + lyrics_scroll_offset) < static_cast<int>(lines.size()); ++i) {
                int idx = i + lyrics_scroll_offset;
                std::string line = lines[idx];
                int line_len = static_cast<int>(line.length());
                if (line_len > lyrics_w) {
                    line = line.substr(0, lyrics_w);
                    line_len = lyrics_w;
                }
                int start_x = lyrics_start + std::max(0, (lyrics_w - line_len) / 2);
                mvwprintw(target_win, i + 1, start_x, "%s", line.c_str());
            }
        }
    }
    wnoutrefresh(target_win);
}

void UI::handle_lyrics_input(int ch) {
    switch (ch) {
        case 27:
        case 'q': case 'Q':
            set_mode(AppMode::PLAYBACK);
            break;
        case 'k': case KEY_UP:
            if (lyrics_scroll_offset > 0) lyrics_scroll_offset--;
            break;
        case 'j': case KEY_DOWN:
            lyrics_scroll_offset++;
            break;
    }
}

void UI::draw_queue() {
    werase(main_win);
    draw_borders(main_win, "PLAY QUEUE");
    
    int height, width;
    getmaxyx(main_win, height, width);
    (void)width;
    
    if (play_queue.empty()) {
        mvwprintw(main_win, 2, 2, "Queue is empty.");
    } else {
        int max_visible = height - 4;
        for (int i = 0; i < max_visible && (i + scroll_offset) < static_cast<int>(play_queue.size()); ++i) {
            int actual_idx = i + scroll_offset;
            const auto& song = play_queue[actual_idx];
            
            if (actual_idx == selection_index) {
                wattron(main_win, COLOR_PAIR(4) | A_BOLD);
            }
            if (actual_idx == queue_index) {
                mvwprintw(main_win, i + 2, 2, "> %s (%s)", song.title.c_str(), song.duration.c_str());
            } else {
                mvwprintw(main_win, i + 2, 2, "  %s (%s)", song.title.c_str(), song.duration.c_str());
            }
            if (actual_idx == selection_index) {
                wattroff(main_win, COLOR_PAIR(4) | A_BOLD);
            }
        }
    }
    wnoutrefresh(main_win);
}

void UI::handle_queue_input(int ch) {
    int height, width;
    getmaxyx(main_win, height, width);
    (void)width;
    int list_h = height - 4;

    switch (ch) {
        case 27: 
            set_mode(AppMode::PLAYBACK); 
            break;
        case 'k': case KEY_UP: 
            if (selection_index > 0) {
                selection_index--;
                if (selection_index < scroll_offset) {
                    scroll_offset = selection_index;
                }
            }
            break;
        case 'j': case KEY_DOWN: 
            if (selection_index < static_cast<int>(play_queue.size()) - 1) {
                selection_index++;
                if (list_h > 0 && selection_index >= scroll_offset + list_h) {
                    scroll_offset = selection_index - list_h + 1;
                }
            }
            break;
        case 10: // Enter
            if (!play_queue.empty() && selection_index < static_cast<int>(play_queue.size())) {
                const auto& song = play_queue[selection_index];
                if (is_url(song.url) && !is_online()) {
                    show_message("Network unavailable. Cannot play online track.");
                    break;
                }
                queue_index = selection_index;
                track_retry_count = 0;
                current_playback_source = PlaybackSource::QUEUE;
                show_message("Playing: " + song.title);
                start_track_playback(song.title, song.url);
                set_mode(AppMode::PLAYBACK);
            }
            break;
        case 'd': case 'D':
            if (!play_queue.empty() && selection_index < static_cast<int>(play_queue.size())) {
                play_queue.erase(play_queue.begin() + selection_index);
                if (queue_index == selection_index) {
                    // Current song was deleted
                    queue_index = -1;
                } else if (queue_index > selection_index) {
                    queue_index--;
                }
                if (selection_index >= static_cast<int>(play_queue.size()) && selection_index > 0) {
                    selection_index--;
                }
                show_message("Track removed from queue.");
            }
            break;
    }
}

void UI::play_next() {
    if (play_queue.empty()) return;
    int next_index = queue_index + 1;
    if (next_index < static_cast<int>(play_queue.size())) {
        const auto& song = play_queue[next_index];
        if (is_url(song.url) && !is_online()) {
            show_message("Network unavailable: Paused at " + song.title);
            return;
        }
        queue_index = next_index;
        track_retry_count = 0;
        show_message("Playing: " + song.title);
        start_track_playback(song.title, song.url);
    } else {
        queue_index = -1;
        show_message("Reached end of queue.");
    }
}

void UI::play_previous() {
    if (player.get_position() > 3.0) {
        player.seek(0);
        return;
    }
    if (queue_index > 0 && queue_index <= static_cast<int>(play_queue.size())) {
        int prev_index = queue_index - 1;
        const auto& song = play_queue[prev_index];
        if (is_url(song.url) && !is_online()) {
            show_message("Network unavailable: Cannot play " + song.title);
            return;
        }
        queue_index = prev_index;
        track_retry_count = 0;
        show_message("Playing previous: " + song.title);
        start_track_playback(song.title, song.url);
    } else {
        player.seek(0);
    }
}

std::string UI::get_user_input(const std::string& prompt) {
    int height, width;
    getmaxyx(stdscr, height, width);
    
    int win_h = 5;
    int win_w = std::min(50, width - 4);
    if (win_w < 20) win_w = 20;
    int start_y = (height - win_h) / 2;
    int start_x = (width - win_w) / 2;
    
    WINDOW* input_win = newwin(win_h, win_w, start_y, start_x);
    wbkgd(input_win, COLOR_PAIR(1)); 
    box(input_win, 0, 0);
    
    mvwprintw(input_win, 0, 2, " %s ", prompt.c_str());
    mvwprintw(input_win, 2, 2, "> ");
    wnoutrefresh(input_win);
    doupdate();
    
    curs_set(1);
    std::string input;
    int ch;
    
    while (true) {
        ch = wgetch(input_win);
        if (ch == 27) { // ESC
            input.clear();
            break;
        } else if (ch == 10) { // Enter
            break;
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            if (!input.empty()) {
                input.pop_back();
                mvwprintw(input_win, 2, 4, "%s ", input.c_str());
                wmove(input_win, 2, 4 + static_cast<int>(input.length()));
                wnoutrefresh(input_win);
                doupdate();
            }
        } else if (isprint(ch)) {
            if (static_cast<int>(input.length()) < win_w - 6) {
                input += static_cast<char>(ch);
                mvwprintw(input_win, 2, 4, "%s", input.c_str());
                wnoutrefresh(input_win);
                doupdate();
            }
        }
    }
    
    curs_set(0);
    delwin(input_win);
    
    clear();
    refresh();
    draw(); 
    
    return input;
}

bool UI::confirm_quit() {
    int height, width;
    getmaxyx(stdscr, height, width);
    
    int win_h = 7;
    int win_w = std::min(46, width - 4);
    if (win_w < 32) win_w = 32;
    int start_y = (height - win_h) / 2;
    int start_x = (width - win_w) / 2;
    
    WINDOW* confirm_win = newwin(win_h, win_w, start_y, start_x);
    keypad(confirm_win, TRUE);

    int selected = 1; // Default to NO for safe navigation
    int ch;

    const std::string prompt = "Wanna quit listening?";
    int prompt_x = (win_w - static_cast<int>(prompt.length())) / 2;
    if (prompt_x < 2) prompt_x = 2;

    curs_set(0);

    while (true) {
        werase(confirm_win);
        wbkgd(confirm_win, COLOR_PAIR(1));
        box(confirm_win, 0, 0);

        // Title on top border
        wattron(confirm_win, COLOR_PAIR(1) | A_BOLD);
        mvwprintw(confirm_win, 0, 2, " Confirmation ");
        wattroff(confirm_win, COLOR_PAIR(1) | A_BOLD);

        // Prompt question in center
        wattron(confirm_win, COLOR_PAIR(1) | A_BOLD);
        mvwprintw(confirm_win, 2, prompt_x, "%s", prompt.c_str());
        wattroff(confirm_win, COLOR_PAIR(1) | A_BOLD);

        // Symmetrical button placement
        int btn_yes_x = win_w / 2 - 11;
        int btn_no_x = win_w / 2 + 3;

        if (selected == 0) { // YES highlighted
            wattron(confirm_win, COLOR_PAIR(4) | A_BOLD | A_REVERSE);
            mvwprintw(confirm_win, 4, btn_yes_x, "  YES  ");
            wattroff(confirm_win, COLOR_PAIR(4) | A_BOLD | A_REVERSE);

            wattron(confirm_win, COLOR_PAIR(1));
            mvwprintw(confirm_win, 4, btn_no_x, "[  NO  ]");
            wattroff(confirm_win, COLOR_PAIR(1));
        } else { // NO highlighted
            wattron(confirm_win, COLOR_PAIR(1));
            mvwprintw(confirm_win, 4, btn_yes_x, "[ YES ]");
            wattroff(confirm_win, COLOR_PAIR(1));

            wattron(confirm_win, COLOR_PAIR(2) | A_BOLD | A_REVERSE);
            mvwprintw(confirm_win, 4, btn_no_x, "   NO   ");
            wattroff(confirm_win, COLOR_PAIR(2) | A_BOLD | A_REVERSE);
        }

        const std::string hint = "[< >] Choose  [Enter] Confirm";
        int hint_x = (win_w - static_cast<int>(hint.length())) / 2;
        if (hint_x < 1) hint_x = 1;
        wattron(confirm_win, A_DIM);
        mvwprintw(confirm_win, 5, hint_x, "%s", hint.c_str());
        wattroff(confirm_win, A_DIM);

        wnoutrefresh(confirm_win);
        doupdate();

        ch = wgetch(confirm_win);
        if (ch == 27) { // ESC -> cancel
            selected = 1; // NO
            break;
        } else if (ch == 10) { // Enter -> confirm choice
            break;
        } else if (ch == 'y' || ch == 'Y') {
            selected = 0;
            break;
        } else if (ch == 'n' || ch == 'N') {
            selected = 1;
            break;
        } else if (ch == KEY_LEFT || ch == KEY_RIGHT || ch == KEY_UP || ch == KEY_DOWN || 
                   ch == '\t' || ch == 'h' || ch == 'l' || ch == 'j' || ch == 'k') {
            selected = (selected == 0) ? 1 : 0;
        }
    }

    delwin(confirm_win);
    clear();
    refresh();
    draw();

    return (selected == 0);
}

void UI::fetch_current_lyrics(std::string title_override, std::string url_override) {
    (void)url_override;
    std::string title = title_override;
    if (title.empty()) {
        title = player.get_metadata("media-title");
        if (title.empty()) title = player.get_metadata("filename");
    }
    
    if (title.empty()) {
        current_lyrics_data = {"Song title missing.", {}, false};
        return;
    }

    current_lyrics_title = title;

    // Strip trailing file extension if present
    size_t last_dot = title.find_last_of('.');
    if (last_dot != std::string::npos && last_dot > title.length() - 5) {
        title = title.substr(0, last_dot);
    }
    
    std::string artist;
    std::string song_title = title;
    
    // Check "Artist - Title" format
    size_t dash_pos = title.find(" - ");
    if (dash_pos != std::string::npos) {
        artist = title.substr(0, dash_pos);
        song_title = title.substr(dash_pos + 3);
    } else {
        artist = player.get_metadata("artist");
    }

    if (discord_rpc) {
        discord_rpc->update_presence(song_title, artist);
    }
    
    show_message("Fetching lyrics...");
    wnoutrefresh(help_win);
    doupdate(); 
    
    current_lyrics_data = lyrics_manager.fetch_lyrics(artist, song_title);
    lyrics_scroll_offset = 0;
    lyrics_auto_scroll = true;
}

void UI::start_track_playback(const std::string& title, const std::string& url) {
    try {
        last_played_path = url;
        player.load(url);
        player.set_property("force-media-title", title);
        player.play();
        fetch_current_lyrics(title, url);
    } catch (const std::exception& e) {
        show_message(std::string("Playback error: ") + e.what());
    }
}

void UI::load_themes() {
    themes.push_back({"Midnight", COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_RED, -1, COLOR_CYAN, COLOR_BLACK});
    themes.push_back({"Matrix", COLOR_GREEN, COLOR_GREEN, COLOR_GREEN, COLOR_RED, -1, COLOR_GREEN, COLOR_BLACK});
    themes.push_back({"Nord", COLOR_CYAN, COLOR_BLUE, COLOR_WHITE, COLOR_RED, -1, COLOR_CYAN, COLOR_BLACK});
    themes.push_back({"HyDE", COLOR_MAGENTA, COLOR_CYAN, COLOR_MAGENTA, COLOR_WHITE, -1, COLOR_MAGENTA, COLOR_BLACK});
}

void UI::apply_theme() {
    if (themes.empty()) return;
    const Theme& t = themes[current_theme_idx];
    init_pair(1, t.border_color, t.bg_color);
    init_pair(2, t.progress_color, t.bg_color);
    init_pair(3, t.visualizer_color, t.bg_color);
    init_pair(4, t.alert_color, t.bg_color);
    init_pair(5, t.bg_color, t.bg_color);
    init_pair(6, t.selected_fg_color, t.selected_bg_color);

    // Multi-tier gradient colors for visualizer
    init_pair(7, t.visualizer_color, t.bg_color);     // Tier 1: Base / Low
    init_pair(8, t.progress_color, t.bg_color);       // Tier 2: Mid
    init_pair(9, t.alert_color, t.bg_color);          // Tier 3: High / Peak
    init_pair(10, COLOR_WHITE, t.bg_color);           // Tier 4: Peak caps
}

void UI::cycle_theme() {
    if (themes.empty()) return;
    current_theme_idx = (current_theme_idx + 1) % themes.size();
    apply_theme();
    show_message("Theme: " + themes[current_theme_idx].name);
    save_state();
    clear();
    refresh();
}

void UI::cycle_visualizer() {
    int vmode = static_cast<int>(current_visualizer_mode);
    vmode = (vmode + 1) % 3; // Cycle: Cava Wave -> Neon Flame -> Stereo Bars
    current_visualizer_mode = static_cast<VisualizerMode>(vmode);
    
    std::string name = "Cava Wave";
    if (current_visualizer_mode == VisualizerMode::NEON_FLAME) name = "Neon Flame";
    else if (current_visualizer_mode == VisualizerMode::STEREO_BARS) name = "Stereo Bars";
    
    show_message("Visualizer: " + name);
    save_state();
}

void UI::load_saved_settings() {
    std::string state_file = get_vibe_dir() + "/state.ini";
    std::ifstream in(state_file);
    if (!in.is_open()) {
        const char* home = getenv("HOME");
        if (home) {
            in.open(std::string(home) + "/.vibe-fi-state.ini");
        }
    }
    if (!in.is_open()) return;

    std::string line;
    while (std::getline(in, line)) {
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            if (key == "theme") {
                for (size_t i = 0; i < themes.size(); ++i) {
                    if (themes[i].name == val) {
                        current_theme_idx = static_cast<int>(i);
                        break;
                    }
                }
            } else if (key == "visualizer") {
                int vm = safe_stoi(val, 0);
                if (vm >= 0 && vm < 3) {
                    current_visualizer_mode = static_cast<VisualizerMode>(vm);
                }
            } else if (key == "autoplay") {
                autoplay_enabled = (val == "1" || val == "true");
            } else if (key == "available_update") {
                cached_available_update = val;
            } else if (key == "last_update_check") {
                last_update_check_time = safe_stoll(val, 0);
            } else if (key == "update_dismissed") {
                update_dismissed_version = val;
            }
        }
    }
}

void UI::save_state() {
    std::string state_file = get_vibe_dir() + "/state.ini";
    std::ofstream out(state_file);
    if (out.is_open()) {
        std::string current_title = player.get_metadata("media-title");
        if (current_title.empty()) current_title = player.get_metadata("filename");
        if (current_title.empty()) current_title = current_lyrics_title;

        out << "path=" << last_played_path << "\n";
        out << "title=" << current_title << "\n";
        out << "position=" << player.get_position() << "\n";
        out << "volume=" << player.get_volume() << "\n";
        out << "playlist=" << (is_playing_from_playlist ? current_playlist_name : "") << "\n";
        out << "index=" << queue_index << "\n";
        if (current_theme_idx >= 0 && current_theme_idx < static_cast<int>(themes.size())) {
            out << "theme=" << themes[current_theme_idx].name << "\n";
        }
        out << "visualizer=" << static_cast<int>(current_visualizer_mode) << "\n";
        out << "autoplay=" << (autoplay_enabled ? "1" : "0") << "\n";
        if (!cached_available_update.empty()) {
            out << "available_update=" << cached_available_update << "\n";
        }
        if (last_update_check_time > 0) {
            out << "last_update_check=" << last_update_check_time << "\n";
        }
        if (!update_dismissed_version.empty()) {
            out << "update_dismissed=" << update_dismissed_version << "\n";
        }
    }
}

void UI::notify_update_available(const std::string& version) {
    std::lock_guard<std::mutex> lock(update_notification_mutex);
    cached_available_update = version;
    pending_update_version = version;
    has_pending_update_notification.store(true);
}

void UI::load_state() {
    std::string state_file = get_vibe_dir() + "/state.ini";
    std::ifstream in(state_file);
    if (!in.is_open()) {
        const char* home = getenv("HOME");
        if (home) {
            in.open(std::string(home) + "/.vibe-fi-state.ini");
        }
    }
    
    if (!in.is_open()) {
        show_message("No saved session found.");
        return;
    }
    
    std::string line;
    std::string path;
    std::string saved_title;
    double position = 0.0;
    int volume = 100;
    std::string playlist;
    int index = -1;
    
    while (std::getline(in, line)) {
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            if (key == "path") path = val;
            else if (key == "title") saved_title = val;
            else if (key == "position") position = safe_stod(val, 0.0);
            else if (key == "volume") volume = safe_stoi(val, 100);
            else if (key == "playlist") playlist = val;
            else if (key == "index") index = safe_stoi(val, -1);
        }
    }
    
    if (!path.empty()) {
        if (is_url(path) && !is_online()) {
            show_message("Network unavailable. Cannot resume online track.");
            return;
        }
        show_message("Resuming session...");
        doupdate();

        // Restore playlist context and recover track title if not explicitly stored
        if (!playlist.empty()) {
            current_playlist_name = playlist;
            playing_playlist_name = playlist;
            current_playlist_songs = playlist_manager.get_playlist_songs(playlist);
            play_queue = current_playlist_songs;
            is_playing_from_playlist = true;
            current_playback_source = PlaybackSource::PLAYLIST;
            if (index >= 0 && index < static_cast<int>(play_queue.size())) {
                if (saved_title.empty()) {
                    saved_title = play_queue[index].title;
                }
            }
        } else {
            is_playing_from_playlist = false;
            playing_playlist_name.clear();
            current_playback_source = PlaybackSource::NONE;
        }

        player.set_property("start", std::to_string(position));
        if (!saved_title.empty()) {
            player.set_property("force-media-title", saved_title);
        }
        player.load(path);
        player.set_property("start", "0");
        player.set_volume(volume);
        last_played_path = path;
        queue_index = index;

        if (!saved_title.empty()) {
            fetch_current_lyrics(saved_title, path);
        } else {
            current_lyrics_title.clear();
            current_lyrics_data = {"Fetching lyrics...", {}, false};
        }
        set_mode(AppMode::PLAYBACK);
    } else {
        show_message("Saved state was empty.");
    }
}
