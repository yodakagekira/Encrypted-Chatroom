#include "ets_server/server.hpp"
#include "ets_server/connection.hpp"
#include "ets_server/chat_hub.hpp"
#include "ets_server/room_manager.hpp"
#include "ets_server/server_config.hpp"   

#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

namespace ets_server {

static void safe_close_fd(int& fd) noexcept {
    if (fd >= 0) { ::close(fd); fd = -1; }
}

Connection::Connection(int fd, std::string peer, ets::CryptoContext crypto)
    : fd_(fd), peer_(std::move(peer)), crypto_(std::move(crypto))
{
    inbuf_.reserve(16 * 1024);
    outbuf_.reserve(16 * 1024);
}

Connection::~Connection() { close_now(); }

Connection::Connection(Connection&& o) noexcept
    : fd_(o.fd_), peer_(std::move(o.peer_)), closed_(o.closed_),
      crypto_(std::move(o.crypto_)), inbuf_(std::move(o.inbuf_)),
      outbuf_(std::move(o.outbuf_)), out_off_(o.out_off_),
      wants_write_(o.wants_write_), username_(std::move(o.username_)),
      room_(std::move(o.room_))
{
    o.fd_ = -1; o.closed_ = true; o.out_off_ = 0; o.wants_write_ = false;
}

Connection& Connection::operator=(Connection&& o) noexcept {
    if (this == &o) return *this;
    close_now();
    fd_ = o.fd_; peer_ = std::move(o.peer_); closed_ = o.closed_;
    crypto_ = std::move(o.crypto_); inbuf_ = std::move(o.inbuf_);
    outbuf_ = std::move(o.outbuf_); out_off_ = o.out_off_;
    wants_write_ = o.wants_write_; username_ = std::move(o.username_);
    room_ = std::move(o.room_);
    o.fd_ = -1; o.closed_ = true; o.out_off_ = 0; o.wants_write_ = false;
    return *this;
}

void Connection::close_now() noexcept {
    if (closed_) return;
    closed_ = true;
    safe_close_fd(fd_);
    inbuf_.clear(); outbuf_.clear();
    out_off_ = 0; wants_write_ = false;
}

bool Connection::on_readable(const OnMessage& on_message) {
    if (closed_ || !read_into_buffer() || !process_frames(on_message)) {
        close_now();
        return false;
    }
    if (inbuf_.size() > MAX_INBUF) { close_now(); return false; }
    return true;
}

bool Connection::on_writable() {
    if (closed_) return false;
    if (!flush_out_buffer()) { close_now(); return false; }
    return true;
}

bool Connection::queue_message(ets::MessageType type, std::string_view plaintext) {
    if (closed_) return false;

    std::vector<std::uint8_t> frame;
    if (!ets::encode_message(type, plaintext, crypto_, frame)) return false;

    const size_t old = outbuf_.size();
    outbuf_.resize(old + frame.size());
    std::memcpy(outbuf_.data() + old, frame.data(), frame.size());
    wants_write_ = true;
    return true;
}

// -------- internal helpers --------

bool Connection::read_into_buffer() {
    std::uint8_t tmp[READ_CHUNK];
    while (true) {
        ssize_t n = ::recv(fd_, tmp, sizeof(tmp), 0);
        if (n > 0) {
            size_t old = inbuf_.size();
            inbuf_.resize(old + n);
            std::memcpy(inbuf_.data() + old, tmp, n);
            continue;
        }
        if (n == 0) return false;
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
            if (errno == EINTR) continue;
            return false;
        }
    }
}

bool Connection::process_frames(const OnMessage& on_message) {
    size_t cursor = 0;

    while (true) {
        size_t avail = inbuf_.size() - cursor;
        if (avail < ets::FRAME_HEADER_SIZE) break;

        ets::FrameHeader hdr{};
        if (!ets::read_header(inbuf_.data() + cursor, avail, hdr)) break;
        if (hdr.version != ets::PROTOCOL_VERSION ||
            hdr.length > ets::MAX_ENCRYPTED_PAYLOAD)
            return false;

        size_t frame_total = ets::FRAME_HEADER_SIZE + hdr.length;
        if (avail < frame_total) break;

        ets::FrameHeader decoded_hdr{};
        std::string plaintext;

        if (!ets::decode_message(inbuf_.data() + cursor, frame_total, crypto_, decoded_hdr, plaintext)) {
            if (errno == 0) return true;  
            return false;                
        }   

        if (on_message)
            on_message(*this, static_cast<ets::MessageType>(decoded_hdr.type), plaintext);

        cursor += frame_total;
    }

    if (cursor > 0) {
        size_t rem = inbuf_.size() - cursor;
        if (rem) std::memmove(inbuf_.data(), inbuf_.data() + cursor, rem);
        inbuf_.resize(rem);
    }
    return true;
}

bool Connection::flush_out_buffer() {
    while (out_off_ < outbuf_.size()) {
        size_t rem = outbuf_.size() - out_off_;
        ssize_t n = ::send(fd_, outbuf_.data() + out_off_, rem, 0);

        if (n > 0) { out_off_ += n; continue; }
        if (n == 0) return false;

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            wants_write_ = true;
            return true;
        }
        if (errno == EINTR) continue;
        return false;
    }

    outbuf_.clear();
    out_off_ = 0;
    wants_write_ = false;
    return true;
}

} 
