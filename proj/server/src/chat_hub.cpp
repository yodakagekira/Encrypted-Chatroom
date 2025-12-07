#include "ets_server/server.hpp"
#include "ets_server/connection.hpp"
#include "ets_server/chat_hub.hpp"
#include "ets_server/room_manager.hpp"
#include "ets_server/server_config.hpp"   

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace ets_server {

void ChatHub::add_connection(int fd) {
    // Ensure we have an entry; empty means not yet set.
    if (usernames_.find(fd) == usernames_.end()) {
        usernames_[fd] = "";
    }
}

void ChatHub::remove_connection(int fd) {
    rooms_.remove(fd);
    usernames_.erase(fd);
}

bool ChatHub::set_username(int fd, std::string username) {
    if (username.empty()) return false;
    usernames_[fd] = std::move(username);
    return true;
}

std::string ChatHub::username_of(int fd) const {
    auto it = usernames_.find(fd);
    if (it == usernames_.end() || it->second.empty()) return "anon";
    return it->second;
}

bool ChatHub::join_room(int fd, const std::string& room) {
    if (room.empty()) return false;
    rooms_.join(fd, room);
    return true;
}

void ChatHub::leave_room(int fd) {
    rooms_.leave(fd);
}

std::string ChatHub::room_of(int fd) const {
    return rooms_.room_of(fd);
}

void ChatHub::broadcast_room(const std::string& room,int from_fd, const std::string& text, const SendFn& send_fn) const
{
    if (!send_fn || room.empty()) return;

    const std::string ts   = timestamp_hhmmss();
    const std::string user = username_of(from_fd);
    const std::string line = format_chat_line(ts, user, text);

    for (int dst_fd : rooms_.members(room)) {
        send_fn(dst_fd, line);
    }
}

void ChatHub::system_to_room(const std::string& room, const std::string& text, const SendFn& send_fn) const
{
    if (!send_fn || room.empty()) return;

    const std::string ts = timestamp_hhmmss();
    const std::string line = "[" + ts + "] [system] " + text;

    for (int dst_fd : rooms_.members(room)) {
        send_fn(dst_fd, line);
    }
}

void ChatHub::system_to_fd(int fd, const std::string& text, const SendFn& send_fn) const
{
    if (!send_fn) return;
    const std::string ts = timestamp_hhmmss();
    const std::string line = "[" + ts + "] [system] " + text;
    send_fn(fd, line);
}

std::string ChatHub::timestamp_hhmmss() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const std::time_t t = system_clock::to_time_t(now);

    std::tm tm{};
    localtime_r(&t, &tm);
    tm = *std::localtime(&t);

    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(2) << tm.tm_hour << ":"
        << std::setw(2) << tm.tm_min  << ":"
        << std::setw(2) << tm.tm_sec;
    return oss.str();
}

std::string ChatHub::format_chat_line(const std::string& ts, const std::string& user, const std::string& text)
{
    // Example: [14:32:10] Alice: hello
    return "[" + ts + "] " + user + ": " + text;
}

} 
