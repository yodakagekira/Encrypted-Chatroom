#include "ets_server/server.hpp"
#include "ets_server/connection.hpp"
#include "ets_server/chat_hub.hpp"
#include "ets_server/room_manager.hpp"
#include "ets_server/server_config.hpp"   

namespace ets_server {

void RoomManager::join(int fd, const std::string& room) {
    if (room.empty()) return;

    // If fd already has a room, leave it first (unless same room).
    auto it = fd_to_room_.find(fd);
    if (it != fd_to_room_.end()) {
        if (it->second == room) return;
        leave(fd);
    }

    rooms_[room].insert(fd);
    fd_to_room_[fd] = room;
}

void RoomManager::leave(int fd) {
    auto it = fd_to_room_.find(fd);
    if (it == fd_to_room_.end()) return;

    const std::string room = it->second;
    fd_to_room_.erase(it);

    auto rit = rooms_.find(room);
    if (rit == rooms_.end()) return;

    rit->second.erase(fd);
    cleanup_room_if_empty(room);
}

std::string RoomManager::room_of(int fd) const {
    auto it = fd_to_room_.find(fd);
    if (it == fd_to_room_.end()) return {};
    return it->second;
}

std::vector<int> RoomManager::members(const std::string& room) const {
    std::vector<int> out;
    auto it = rooms_.find(room);
    if (it == rooms_.end()) return out;

    out.reserve(it->second.size());
    for (int fd : it->second) out.push_back(fd);
    return out;
}

std::size_t RoomManager::room_size(const std::string& room) const {
    auto it = rooms_.find(room);
    if (it == rooms_.end()) return 0;
    return it->second.size();
}

void RoomManager::cleanup_room_if_empty(const std::string& room) {
    auto it = rooms_.find(room);
    if (it != rooms_.end() && it->second.empty()) {
        rooms_.erase(it);
    }
}

} 
