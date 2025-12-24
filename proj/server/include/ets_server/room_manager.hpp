#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ets_server/chat_hub.hpp"


namespace ets_server {

class RoomManager {
public:
    // Join a room (leaves any previous room automatically).
    void join(int fd, const std::string& room);

    // Leave current room (if any).
    void leave(int fd);

    // Remove a connection completely (same as leave).
    void remove(int fd) { leave(fd); }

    // Get current room for fd; empty if none.
    std::string room_of(int fd) const;

    // Snapshot list of fds in a room.
    std::vector<int> members(const std::string& room) const;

    std::size_t room_size(const std::string& room) const;

private:
    std::unordered_map<std::string, std::unordered_set<int>> rooms_; // room, fds
    std::unordered_map<int, std::string> fd_to_room_;                // fd, room

    void cleanup_room_if_empty(const std::string& room); 
};

} 
