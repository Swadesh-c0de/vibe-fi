#ifndef PLAYER_HPP
#define PLAYER_HPP

#include <string>
#include <mpv/client.h>

class Player {
public:
    Player();
    ~Player();

    void load(const std::string& path, const std::string& mode = "replace");
    void play();
    void pause();
    void toggle_pause();
    void stop();
    void seek(double seconds);
    
    bool is_playing();
    bool is_paused();
    bool is_idle();
    double get_position();
    double get_duration();
    int get_volume();
    void set_volume(int volume);
    std::string get_metadata(const std::string& key);
    void set_property(const std::string& name, const std::string& value);

private:
    mpv_handle* mpv;
    void check_error(int status);
};

#endif // PLAYER_HPP
