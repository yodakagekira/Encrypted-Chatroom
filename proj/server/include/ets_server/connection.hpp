#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "include/ets/protocol.hpp" 
#include "include/ets/crypto_context.hpp"
#include "ets_server/chat_hub.hpp"


namespace ets_server {

class Connection {
public:
    using OnMessage = std::function<void(Connection&, ets::MessageType, const std::string& /*plaintext*/)>;

    explicit Connection(int fd, std::string peer, ets::CryptoContext crypto);
    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    Connection(Connection&&) noexcept;
    Connection& operator=(Connection&&) noexcept;

    int fd() const noexcept { return fd_; }
    const std::string& peer() const noexcept { return peer_; }

    bool is_closed() const noexcept { return closed_; }
    bool wants_write() const noexcept { return wants_write_; }

    // Stage2 identity/room tracking
    const std::string& username() const noexcept { return username_; }
    const std::string& room() const noexcept { return room_; }
    void set_username(std::string u) { username_ = std::move(u); }
    void set_room(std::string r) { room_ = std::move(r); }

    // If you want to swap keys after KDS/handshake
    void set_crypto(ets::CryptoContext crypto) { crypto_ = std::move(crypto); }
    bool on_readable(const OnMessage& on_message);
    bool on_writable();

    // Queue plaintext for sending (will encrypt+MAC+frame).
    bool queue_message(ets::MessageType type, std::string_view plaintext);

    void close_now() noexcept;

private:
    int fd_{-1};
    std::string peer_;
    bool closed_{false};

    ets::CryptoContext crypto_;

    // Input buffering for stream reassembly
    std::vector<std::uint8_t> inbuf_;

    // Output buffering (single contiguous buffer + offset)
    std::vector<std::uint8_t> outbuf_;
    std::size_t out_off_{0};
    bool wants_write_{false};

    // Chat identity
    std::string username_;
    std::string room_;

    bool read_into_buffer();
    bool process_frames(const OnMessage& on_message);
    bool flush_out_buffer();

    static constexpr std::size_t READ_CHUNK = 4096;
    static constexpr std::size_t MAX_INBUF  = ets::FRAME_HEADER_SIZE + ets::MAX_ENCRYPTED_PAYLOAD * 2;
};

} // namespace ets_server
