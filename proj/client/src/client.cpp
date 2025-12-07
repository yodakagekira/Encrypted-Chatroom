#include "client.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>

namespace ets_client {

Client::Client() = default;

Client::~Client() {
    stop();
}

bool Client::connect_to(const std::string& host, int port) {
    if (fd_ >= 0) close_now();

    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        close_now();
        return false;
    }

    if (::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close_now();
        return false;
    }

    return true;
}

void Client::set_shared_secret(std::string secret) {
    crypto_ = ets::CryptoContext::from_shared_secret(secret);
}

int Client::run() {
    if (fd_ < 0) {
        std::lock_guard<std::mutex> lock(io_mutex_);
        std::cerr << "Client not connected.\n";
        return 1;
    }

    running_.store(true, std::memory_order_release);

    // Receiver thread: prints incoming broadcasts while you type.
    rx_thread_ = std::thread([this]{ rx_loop(); });

    {
        std::lock_guard<std::mutex> lock(io_mutex_);
        std::cout << "Commands: HELLO <name> | JOIN <room> | DISC | /quit\n";
    }

    while (running_.load(std::memory_order_acquire)) {
        std::string line;
        {
            std::lock_guard<std::mutex> lock(io_mutex_);
            std::cout << "> " << std::flush;
        }

        if (!std::getline(std::cin, line)) break;
        trim_cr(line);

        if (!send_line(line)) {
            std::lock_guard<std::mutex> lock(io_mutex_);
            std::cerr << "Send failed.\n";
            break;
        }

        // local quit shortcut
        if (line == "/quit" || line == "/exit" || line == "DISC") {
            break;
        }
    }

    stop();
    return 0;
}

void Client::stop() {
    bool was_running = running_.exchange(false, std::memory_order_acq_rel);
    (void)was_running;

    // Closing the socket will unblock rx thread recv
    close_now();

    if (rx_thread_.joinable()) {
        rx_thread_.join();
    }
}

void Client::close_now() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool Client::send_line(const std::string& line) {
    auto [type, payload] = parse_command(line);

    std::vector<std::uint8_t> frame;
    if (!ets::encode_message(type, payload, crypto_, frame)) {
        return false;
    }

    if (!send_frame(frame)) return false;
    return true;
}

bool Client::send_frame(const std::vector<std::uint8_t>& frame) {
    if (fd_ < 0) return false;
    return send_all(fd_, frame.data(), frame.size());
}

bool Client::send_all(int fd, const std::uint8_t* data, std::size_t len) {
    std::size_t off = 0;
    while (off < len) {
        const ssize_t n = ::send(fd, data + off, len - off, 0);
        if (n > 0) {
            off += static_cast<std::size_t>(n);
            continue;
        }
        if (n == 0) return false;
        if (errno == EINTR) continue;
        return false;
    }
    return true;
}

void Client::rx_loop() {
    while (running_.load(std::memory_order_acquire)) {
        ets::FrameHeader hdr{};
        std::string plaintext;

        if (!recv_one_frame(hdr, plaintext)) {
            running_.store(false, std::memory_order_release);
            std::lock_guard<std::mutex> lock(io_mutex_);
            std::cout << "\n[system] disconnected.\n";
            break;
        }

        // Print any incoming plaintext (already timestamped by server in your design)
        {
            std::lock_guard<std::mutex> lock(io_mutex_);
            std::cout << "\n" << plaintext << "\n";
        }
    }
}

bool Client::recv_one_frame(ets::FrameHeader& hdr_out, std::string& plaintext_out) {
    if (fd_ < 0) return false;

    std::uint8_t hdr_buf[ets::FRAME_HEADER_SIZE];
    if (!recv_exact(fd_, hdr_buf, ets::FRAME_HEADER_SIZE)) return false;

    if (!ets::read_header(hdr_buf, ets::FRAME_HEADER_SIZE, hdr_out)) return false;
    if (hdr_out.version != ets::PROTOCOL_VERSION) return false;
    if (hdr_out.length > ets::MAX_ENCRYPTED_PAYLOAD) return false;

    std::vector<std::uint8_t> full;
    full.resize(ets::FRAME_HEADER_SIZE + hdr_out.length);
    std::memcpy(full.data(), hdr_buf, ets::FRAME_HEADER_SIZE);

    if (!recv_exact(fd_, full.data() + ets::FRAME_HEADER_SIZE, hdr_out.length)) return false;

    ets::FrameHeader decoded{};
    if (!ets::decode_message(full.data(), full.size(), crypto_, decoded, plaintext_out)) return false;
    hdr_out = decoded;
    return true;
}

bool Client::recv_exact(int fd, std::uint8_t* dst, std::size_t n) {
    std::size_t off = 0;
    while (off < n) {
        const ssize_t r = ::recv(fd, dst + off, n - off, 0);
        if (r > 0) {
            off += static_cast<std::size_t>(r);
            continue;
        }
        if (r == 0) return false;
        if (errno == EINTR) continue;
        return false;
    }
    return true;
}

bool Client::starts_with(std::string_view s, std::string_view pfx) {
    return s.size() >= pfx.size() && std::memcmp(s.data(), pfx.data(), pfx.size()) == 0;
}

void Client::trim_cr(std::string& s) {
    if (!s.empty() && s.back() == '\r') s.pop_back();
}

std::pair<ets::MessageType, std::string> Client::parse_command(const std::string& line) {
    std::string_view sv(line);

    if (starts_with(sv, "HELLO ")) {return {ets::MessageType::Hello, std::string(sv.substr(6))}; }
    if (starts_with(sv, "JOIN ")) { return {ets::MessageType::Join, std::string(sv.substr(5))}; }
    if (sv == "DISC" || sv == "/quit" || sv == "/exit") {return {ets::MessageType::Disc, ""}; }
    if (sv == "/rooms")  return {ets::MessageType::RoomN, ""};
    if (sv == "/users")  return {ets::MessageType::UserN, ""};

    return {ets::MessageType::Chat, std::string(sv)};
    }
}
