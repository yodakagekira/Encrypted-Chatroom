#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "include/ets/crypto_context.hpp"
#include "include/ets/protocol.hpp"

namespace ets_client {

class Client {
public:
    Client();
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    bool connect_to(const std::string& host, int port);

    void set_shared_secret(std::string secret);

    // Start receiver thread + interactive prompt loop.
    // Returns when user disconnects or server closes.
    int run();

    // Stop threads and close socket.
    void stop();

private:
    int fd_{-1};
    std::atomic<bool> running_{false};

    ets::CryptoContext crypto_{ets::CryptoContext::from_shared_secret("changeme_shared_secret")};

    std::thread rx_thread_;
    std::mutex io_mutex_;


    // Sender helpers
    bool send_line(const std::string& line);
    static std::pair<ets::MessageType, std::string> parse_command(const std::string& line);

    bool send_frame(const std::vector<std::uint8_t>& frame);
    static bool send_all(int fd, const std::uint8_t* data, std::size_t len);

    // Receiver thread
    void rx_loop();
    bool recv_one_frame(ets::FrameHeader& hdr_out, std::string& plaintext_out);

    static bool recv_exact(int fd, std::uint8_t* dst, std::size_t n);

    // Utils
    static void trim_cr(std::string& s);
    static bool starts_with(std::string_view s, std::string_view pfx);
    void close_now();
};

} 
