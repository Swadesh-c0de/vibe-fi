#include "discord_rpc.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>

DiscordRPC::DiscordRPC(const std::string& client_id) : client_id(client_id), fd(-1), connected(false) {
    connected = connect_to_discord();
}

DiscordRPC::~DiscordRPC() {
    if (fd != -1) {
        close(fd);
    }
}

bool DiscordRPC::connect_to_discord() {
    const char* runtime_dir = getenv("XDG_RUNTIME_DIR");
    char path[1024];
    
    for (int i = 0; i < 10; ++i) {
        if (runtime_dir) {
            snprintf(path, sizeof(path), "%s/discord-ipc-%d", runtime_dir, i);
        } else {
            snprintf(path, sizeof(path), "/run/user/%d/discord-ipc-%d", getuid(), i);
        }

        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd == -1) continue;

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

        if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            // Handshake
            std::string payload = "{\"v\":1,\"client_id\":\"" + client_id + "\"}";
            send_frame(0, payload);
            return true;
        }

        close(fd);
        fd = -1;
    }
    return false;
}

void DiscordRPC::send_frame(int opcode, const std::string& payload) {
    if (fd == -1) return;

    uint32_t header[2];
    header[0] = opcode;
    header[1] = static_cast<uint32_t>(payload.length());

    write(fd, header, sizeof(header));
    write(fd, payload.c_str(), payload.length());

    // We don't really care about the response for now
    char buffer[1024];
    read(fd, buffer, sizeof(buffer));
}

void DiscordRPC::update_presence(const std::string& song_title, const std::string& artist) {
    if (!connected && !connect_to_discord()) return;

    std::string details = song_title;
    std::string state = artist.empty() ? "on Vibe-Fi" : "by " + artist;
    
    // Sanitize JSON
    auto sanitize = [](std::string s) {
        std::string res;
        for (char c : s) {
            if (c == '"') res += "\\\"";
            else if (c == '\\') res += "\\\\";
            else res += c;
        }
        return res;
    };

    std::ostringstream ss;
    ss << "{"
       << "\"cmd\":\"SET_ACTIVITY\","
       << "\"args\":{"
       << "\"pid\":" << getpid() << ","
       << "\"activity\":{"
       << "\"details\":\"" << sanitize(details) << "\","
       << "\"state\":\"" << sanitize(state) << "\","
       << "\"assets\":{"
       << "\"large_image\":\"vibe_fi_logo\","
       << "\"large_text\":\"Vibe-Fi\""
       << "}"
       << "}"
       << "},"
       << "\"nonce\":\"" << std::chrono::steady_clock::now().time_since_epoch().count() << "\""
       << "}";

    send_frame(1, ss.str());
}

void DiscordRPC::clear_presence() {
    if (!connected) return;

    std::ostringstream ss;
    ss << "{"
       << "\"cmd\":\"SET_ACTIVITY\","
       << "\"args\":{"
       << "\"pid\":" << getpid() << ","
       << "\"activity\":null"
       << "},"
       << "\"nonce\":\"" << std::chrono::steady_clock::now().time_since_epoch().count() << "\""
       << "}";

    send_frame(1, ss.str());
}
