#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "ets_server/connection.hpp"
#include "ets_server/server_config.hpp" 
#include "include/ets/protocol.hpp"

namespace ets_server {

class EpollServer {
public:
    explicit EpollServer(ServerConfig cfg);
    ~EpollServer();

    EpollServer(const EpollServer&) = delete;
    EpollServer& operator=(const EpollServer&) = delete;

    void run();   // blocking event loop
    void stop();  // request shutdown

private:
    ServerConfig cfg_;

    int listen_fd_{-1};
    int epoll_fd_{-1};
    bool running_{false};

    // key: client fd
    std::unordered_map<int, Connection> conns_;


    void setup_listen_socket();
    void setup_epoll();
    void loop();

    void accept_new_connections();
    void handle_event(int fd, std::uint32_t events);

    void close_connection(int fd);

    void update_interest(int fd);
    void ensure_writable(int fd);

    void on_message(Connection& c, ets::MessageType type, const std::string& plaintext);

    void broadcast_room(const std::string& room, const std::string& from_user, const std::string& text);

    static std::string format_peer(const struct sockaddr_in& addr);
    static std::string timestamp_hhmmss();
};

}