#ifndef DISCORD_RPC_HPP
#define DISCORD_RPC_HPP

#include <string>

class DiscordRPC {
public:
    DiscordRPC(const std::string& client_id);
    ~DiscordRPC();

    // Non-copyable
    DiscordRPC(const DiscordRPC&) = delete;
    DiscordRPC& operator=(const DiscordRPC&) = delete;

    void update_presence(const std::string& song_title, const std::string& artist);
    void clear_presence();

private:
    std::string client_id;
    int fd;
    bool connected;

    bool connect_to_discord();
    void close_connection();
    void send_frame(int opcode, const std::string& payload);
};

#endif // DISCORD_RPC_HPP
