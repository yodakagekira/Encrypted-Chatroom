#include "ets_server/server.hpp"
#include "ets_server/connection.hpp"
#include "ets_server/chat_hub.hpp"
#include "ets_server/room_manager.hpp"
#include "ets_server/server_config.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

namespace ets_server {

static void safe_close(int& fd) noexcept { if (fd >= 0) ::close(fd), fd = -1; }
static bool set_nonblocking(int fd) {
    int f = ::fcntl(fd, F_GETFL, 0);
    return (f >= 0 && ::fcntl(fd, F_SETFL, f | O_NONBLOCK) == 0);
}

EpollServer::EpollServer(ServerConfig c) : cfg_(std::move(c)) {}
EpollServer::~EpollServer() {
    stop();
    for (auto& kv : conns_) kv.second.close_now();
    conns_.clear();
    safe_close(epoll_fd_);
    safe_close(listen_fd_);
}

void EpollServer::stop() { running_ = false; }
void EpollServer::run() { setup_listen_socket(); setup_epoll(); running_ = true; loop(); }

void EpollServer::setup_listen_socket() {
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) throw std::runtime_error("socket() failed");

    int opt = 1;
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (!set_nonblocking(listen_fd_)) throw std::runtime_error("fcntl failed");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(cfg_.port);
    if (::inet_pton(AF_INET, cfg_.bind_address.c_str(), &addr.sin_addr) != 1)
        throw std::runtime_error("inet_pton failed");

    if (::bind(listen_fd_, (sockaddr*)&addr, sizeof(addr)) < 0 ||
        ::listen(listen_fd_, 1024) < 0)
        throw std::runtime_error("bind/listen failed");
}

void EpollServer::setup_epoll() {
    epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0) throw std::runtime_error("epoll_create1 failed");

    epoll_event ev{};
    ev.data.fd = listen_fd_;
    ev.events  = EPOLLIN;

    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev) < 0)
        throw std::runtime_error("epoll_ctl ADD failed");
}

void EpollServer::loop() {
    constexpr int MAX_EVENTS = 256;
    std::vector<epoll_event> events(MAX_EVENTS);

    std::cout << "[" << timestamp_hhmmss() << "] Listening on "
              << cfg_.bind_address << ":" << cfg_.port << "\n";

    while (running_) {
        int n = ::epoll_wait(epoll_fd_, events.data(), MAX_EVENTS, -1);
        if (n < 0 && errno != EINTR)
            throw std::runtime_error("epoll_wait failed");

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            (fd == listen_fd_) ? accept_new_connections()
                               : handle_event(fd, events[i].events);
        }
    }
}

void EpollServer::accept_new_connections() {
    for (;;) {
        sockaddr_in ca{};
        socklen_t cl = sizeof(ca);

        int cfd = ::accept4(listen_fd_, (sockaddr*)&ca, &cl,
                            SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (cfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            if (errno == EINTR) continue;
            std::cerr << "accept4: " << std::strerror(errno) << "\n";
            return;
        }

        if (cfg_.max_connections > 0 && (int)conns_.size() >= cfg_.max_connections) {
            ::close(cfd);
            continue;
        }

        ets::CryptoContext crypto = ets::CryptoContext::from_shared_secret(cfg_.shared_secret);
        Connection conn(cfd, format_peer(ca), std::move(crypto));
        conn.set_room("lobby");

        epoll_event ev{};
        ev.data.fd = cfd;
        ev.events  = EPOLLIN | EPOLLRDHUP;

        if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, cfd, &ev) < 0) {
            conn.close_now();
            continue;
        }

        conns_.emplace(cfd, std::move(conn));
        std::cout << "[" << timestamp_hhmmss() << "] New connection fd=" << cfd << "\n";

        auto& c = conns_.at(cfd);
        hub_.add_connection(cfd);
        hub_.join_room(cfd, "lobby");

        c.queue_message(ets::MessageType::Hello, "Welcome. Use HELLO <name>, JOIN <room>.");
        ensure_writable(cfd);
    }
}

void EpollServer::handle_event(int fd, uint32_t events) {
    auto it = conns_.find(fd);
    if (it == conns_.end()) return;
    Connection& c = it->second;

    if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) return close_connection(fd);

    if ((events & EPOLLIN) && !c.on_readable([this](Connection& cc,
                               ets::MessageType t,
                               const std::string& m){
            on_message(cc, t, m);
        }))
        return close_connection(fd);

    if ((events & EPOLLOUT) && !c.on_writable())
        return close_connection(fd);

    update_interest(fd);
}

void EpollServer::close_connection(int fd) {
    auto it = conns_.find(fd);
    if (it == conns_.end()) return;

    std::cout << "[" << timestamp_hhmmss() << "] Closed: "
              << it->second.peer() << " user=" << it->second.username()
              << " room=" << it->second.room() << "\n";

    ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    it->second.close_now();
    hub_.remove_connection(fd);
    hub_.rooms().remove(fd);
    conns_.erase(it);
}

void EpollServer::update_interest(int fd) {
    auto it = conns_.find(fd);
    if (it == conns_.end()) return;

    epoll_event ev{};
    ev.data.fd = fd;
    ev.events  = EPOLLIN | EPOLLRDHUP;
    if (it->second.wants_write())
        ev.events |= EPOLLOUT;

    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) < 0)
        close_connection(fd);
}

void EpollServer::ensure_writable(int fd) {
    auto it = conns_.find(fd);
    if (it != conns_.end() && it->second.wants_write())
        update_interest(fd);
}

static bool starts_with(std::string_view s, std::string_view p) {
    return s.size() >= p.size() && !memcmp(s.data(), p.data(), p.size());
}

void EpollServer::broadcast_room(const std::string& room,
                                 const std::string& from_user,
                                 const std::string& text) {
    hub_.broadcast_room(room, -1, text, [&](int dst_fd, const std::string& msg) {
        auto it = conns_.find(dst_fd);
        if (it != conns_.end()) {
            it->second.queue_message(ets::MessageType::Chat, msg);
            ensure_writable(dst_fd);
        }
    });
}

void EpollServer::on_message(Connection& c, ets::MessageType t, const std::string& msg) {
    int fd = c.fd();

    // Helper to queue a message to this client
    auto send_to_client = [&](ets::MessageType type, std::string_view text) {
        c.queue_message(type, text);
        ensure_writable(fd);
    };

    // Trim \r
    std::string_view payload = msg;
    if (!payload.empty() && payload.back() == '\r') {
        payload.remove_suffix(1);
    }

    switch (t) {
        case ets::MessageType::Hello: {
            if (payload.empty()) {
                send_to_client(ets::MessageType::Chat, "[system] Error: Empty username");
                return;
            }
            std::string username(payload);
            if (username.size() > 32) {
                send_to_client(ets::MessageType::Chat, "[system] Error: Username too long");
                return;
            }
            c.set_username(std::move(username));
            send_to_client(ets::MessageType::Chat, "[system] Username set");
            return;
        }

        case ets::MessageType::Join: {
            if (payload.empty()) {
                send_to_client(ets::MessageType::Chat, "[system] Error: Empty room name");
                return;
            }
            std::string room(payload);
            if (room.size() > 32) {
                send_to_client(ets::MessageType::Chat, "[system] Error: Room name too long");
                return;
            }

            const std::string old_room = c.room().empty() ? "lobby" : c.room();
            c.set_room(std::move(room));

            // Announce join in new room
            broadcast_room(c.room(), "[system]", c.username() + " has joined");

            // Announce leave in old room (if different and not empty)
            if (old_room != c.room() && !old_room.empty()) {
                broadcast_room(old_room, "[system]", c.username() + " has left");
            }
            return;
        }

        case ets::MessageType::Disc:
            send_to_client(ets::MessageType::Chat, "[system] Goodbye!");
            close_connection(fd);
            return;

        case ets::MessageType::Chat: {
            if (payload.empty()) return;  // ignore empty messages

            const std::string room = c.room().empty() ? "lobby" : c.room();
            const std::string user = c.username().empty() ? "anon" : c.username();

            broadcast_room(room, user, std::string(payload));
            return;
        }

        case ets::MessageType::RoomN: {
            std::string rooms = "Available rooms:\n";
            std::unordered_map<std::string, int> room_counts;
            for (const auto& kv : conns_) {
                const std::string r = kv.second.room().empty() ? "lobby" : kv.second.room();
                room_counts[r]++;
            }
            for (const auto& rc : room_counts) {
                rooms += " - " + rc.first + " (" + std::to_string(rc.second) + " users)\n";
            }
            send_to_client(ets::MessageType::RoomN, rooms);
            return;
        }

        case ets::MessageType::UserN: {
            std::string users = "Users in room '" + (c.room().empty() ? "lobby" : c.room()) + "':\n";
            const std::string curr_room = c.room().empty() ? "lobby" : c.room();
            for (const auto& kv : conns_) {
                const Connection& oc = kv.second;
                const std::string oroom = oc.room().empty() ? "lobby" : oc.room();
                if (oroom == curr_room) {
                    users += " - " + (oc.username().empty() ? "anon" : oc.username()) + "\n";
                }
            }
            send_to_client(ets::MessageType::UserN, users);
            return;
        }

        default:
            return;
    }
}

std::string EpollServer::format_peer(const sockaddr_in& a) {
    char ip[INET_ADDRSTRLEN];
    ::inet_ntop(AF_INET, &a.sin_addr, ip, sizeof(ip));
    std::ostringstream o;
    o << ip << ":" << ntohs(a.sin_port);
    return o.str();
}

std::string EpollServer::timestamp_hhmmss() {
    using namespace std::chrono;
    std::time_t t = system_clock::to_time_t(system_clock::now());
    std::tm tm{};
    localtime_r(&t, &tm);

    tm = *localtime(&t);
    std::ostringstream o;
    o << std::setfill('0') << std::setw(2) << tm.tm_hour << ":"
      << std::setw(2) << tm.tm_min  << ":"
      << std::setw(2) << tm.tm_sec;
    return o.str();
}

}
