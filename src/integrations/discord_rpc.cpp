#include "discord_rpc.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <vector>

DiscordRPC::DiscordRPC(const std::string& client_id) : client_id(client_id), fd(-1), connected(false) {
    // Ignore SIGPIPE so unexpected socket closure never crashes the application
    signal(SIGPIPE, SIG_IGN);
    connected = connect_to_discord();
}

DiscordRPC::~DiscordRPC() {
    close_connection();
}

void DiscordRPC::close_connection() {
    if (fd != -1) {
        close(fd);
        fd = -1;
    }
    connected = false;
}

bool DiscordRPC::connect_to_discord() {
    close_connection();

    const char* runtime_dir = getenv("XDG_RUNTIME_DIR");
    const char* tmp_dir = getenv("TMPDIR");
    if (!tmp_dir) tmp_dir = "/tmp";

    std::vector<std::string> search_paths;
    char path[1024];

    for (int i = 0; i < 10; ++i) {
        if (runtime_dir) {
            snprintf(path, sizeof(path), "%s/discord-ipc-%d", runtime_dir, i);
            search_paths.push_back(path);
        }
        snprintf(path, sizeof(path), "/run/user/%d/discord-ipc-%d", getuid(), i);
        search_paths.push_back(path);
        snprintf(path, sizeof(path), "%s/discord-ipc-%d", tmp_dir, i);
        search_paths.push_back(path);
    }

    for (const auto& socket_path : search_paths) {
        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd == -1) continue;

        // Set short timeout to prevent blocking during connection or reads
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

        if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
            // Handshake (Opcode 0)
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

    // Use MSG_NOSIGNAL flag to prevent broken pipe signals
    ssize_t sent_hdr = send(fd, header, sizeof(header), MSG_NOSIGNAL);
    if (sent_hdr < 0) {
        close_connection();
        return;
    }

    ssize_t sent_body = send(fd, payload.c_str(), payload.length(), MSG_NOSIGNAL);
    if (sent_body < 0) {
        close_connection();
        return;
    }

    // Drain any immediate response without blocking
    char buffer[1024];
    recv(fd, buffer, sizeof(buffer), MSG_DONTWAIT);
}

void DiscordRPC::update_presence(const std::string& song_title, const std::string& artist) {
    if (!connected) {
        static std::chrono::steady_clock::time_point last_attempt{};
        auto now = std::chrono::steady_clock::now();
        if (last_attempt.time_since_epoch().count() > 0 &&
            std::chrono::duration_cast<std::chrono::seconds>(now - last_attempt).count() < 10) {
            return;
        }
        last_attempt = now;
        if (!connect_to_discord()) return;
    }

    std::string details = song_title;
    std::string state = artist.empty() ? "on Vibe-Fi" : "by " + artist;

    auto sanitize = [](const std::string& s) {
        std::string res;
        for (char c : s) {
            if (c == '"') res += "\\\"";
            else if (c == '\\') res += "\\\\";
            else if (c == '\n') res += " ";
            else if (c == '\r') res += "";
            else res += c;
        }
        return res;
    };

    auto nonce = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

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
       << "\"nonce\":\"" << nonce << "\""
       << "}";

    send_frame(1, ss.str());
}

void DiscordRPC::clear_presence() {
    if (!connected) return;

    auto nonce = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

    std::ostringstream ss;
    ss << "{"
       << "\"cmd\":\"SET_ACTIVITY\","
       << "\"args\":{"
       << "\"pid\":" << getpid() << ","
       << "\"activity\":null"
       << "},"
       << "\"nonce\":\"" << nonce << "\""
       << "}";

    send_frame(1, ss.str());
}
