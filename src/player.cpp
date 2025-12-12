#include "player.hpp"
#include <stdexcept>
#include <iostream>

Player::Player() {
    mpv = mpv_create();
    if (!mpv) {
        throw std::runtime_error("failed to create mpv context");
    }

    // Set some default options if needed
    check_error(mpv_set_option_string(mpv, "vo", "null")); // Audio only

    check_error(mpv_initialize(mpv));
}

Player::~Player() {
    if (mpv) {
        mpv_terminate_destroy(mpv);
    }
}

void Player::check_error(int status) {
    if (status < 0) {
        throw std::runtime_error(std::string("mpv error: ") + mpv_error_string(status));
    }
}

void Player::load(const std::string& path, const std::string& mode) {
    const char* cmd[] = {"loadfile", path.c_str(), mode.c_str(), NULL};
    check_error(mpv_command(mpv, cmd));
}

void Player::play() {
    int flag = 0;
    check_error(mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &flag));
}

void Player::pause() {
    int flag = 1;
    check_error(mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &flag));
}

void Player::toggle_pause() {
    int flag;
    check_error(mpv_get_property(mpv, "pause", MPV_FORMAT_FLAG, &flag));
    flag = !flag;
    check_error(mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &flag));
}

void Player::stop() {
    const char* cmd[] = {"stop", NULL};
    check_error(mpv_command(mpv, cmd));
}

bool Player::is_playing() {
    int flag;
    if (mpv_get_property(mpv, "pause", MPV_FORMAT_FLAG, &flag) < 0) return false;
    return !flag;
}

bool Player::is_paused() {
    int flag;
    if (mpv_get_property(mpv, "pause", MPV_FORMAT_FLAG, &flag) < 0) return false;
    return flag;
}

bool Player::is_idle() {
    int flag;
    if (mpv_get_property(mpv, "idle-active", MPV_FORMAT_FLAG, &flag) < 0) return true; // Assume idle if error
    return flag;
}

double Player::get_position() {
    double pos;
    if (mpv_get_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &pos) < 0) return 0.0;
    return pos;
}

double Player::get_duration() {
    double dur;
    if (mpv_get_property(mpv, "duration", MPV_FORMAT_DOUBLE, &dur) < 0) return 0.0;
    return dur;
}

int Player::get_volume() {
    double vol;
    if (mpv_get_property(mpv, "volume", MPV_FORMAT_DOUBLE, &vol) < 0) return 0;
    return static_cast<int>(vol);
}

void Player::set_volume(int volume) {
    double vol = static_cast<double>(volume);
    check_error(mpv_set_property(mpv, "volume", MPV_FORMAT_DOUBLE, &vol));
}

void Player::seek(double seconds) {
    std::string seconds_str = std::to_string(seconds);
    const char* cmd[] = {"seek", seconds_str.c_str(), "relative", NULL};
    check_error(mpv_command(mpv, cmd));
}

std::string Player::get_metadata(const std::string& key) {
    char* value = mpv_get_property_string(mpv, key.c_str());
    if (value) {
        std::string result = value;
        mpv_free(value);
        return result;
    }
    return "";
}

void Player::set_property(const std::string& name, const std::string& value) {
    check_error(mpv_set_property_string(mpv, name.c_str(), value.c_str()));
}
