#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "ets_server/room_manager.hpp"

namespace ets_server {

// ChatHub owns *chat state* (usernames + room membership) and provides
// broadcast helpers. It does NOT touch epoll directly; you pass a callback
// responsible for actually sending/queueing the message to a client fd.
class ChatHub {
public:
    using SendFn = std::function<void(int /*dst_fd*/, const std::string& /*plaintext*/)>;

    // Lifecycle
    void add_connection(int fd);
    void remove_connection(int fd);

    // Identity
    bool set_username(int fd, std::string username);
    std::string username_of(int fd) const;

    // Rooms
    bool join_room(int fd, const std::string& room);
    void leave_room(int fd);
    std::string room_of(int fd) const;

    // Broadcast plaintext message to everyone in a room (with timestamp formatting).
    void broadcast_room(const std::string& room, int from_fd, const std::string& text, const SendFn& send_fn) const;

    // System messages
    void system_to_room(const std::string& room, const std::string& text, const SendFn& send_fn) const;

    void system_to_fd(int fd, const std::string& text, const SendFn& send_fn) const;

    RoomManager& rooms() noexcept { return rooms_; }
    const RoomManager& rooms() const noexcept { return rooms_; }

private:
    RoomManager rooms_;
    std::unordered_map<int, std::string> usernames_; // fd -> username (empty => anon)

    static std::string timestamp_hhmmss();
    static std::string format_chat_line(const std::string& ts, const std::string& user, const std::string& text);
};

} 
