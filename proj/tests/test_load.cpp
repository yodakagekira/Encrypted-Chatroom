#include "ets/crypto_context.hpp"
#include "ets/protocol.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <vector>
#include <random>
#include <iostream>
#include <atomic>
#include <cstring>

std::atomic<bool> running{true};

void client_thread(int id, const std::string& secret) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return;
    }

    ets::CryptoContext crypto = ets::CryptoContext::from_shared_secret(secret);

    // HELLO
    std::vector<uint8_t> frame;
    ets::encode_message(ets::MessageType::Hello, "LoadBot_" + std::to_string(id), crypto, frame);
    send(fd, frame.data(), frame.size(), 0);

    // JOIN general
    ets::encode_message(ets::MessageType::Join, "general", crypto, frame);
    send(fd, frame.data(), frame.size(), 0);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> msg_dist(1, 200);
    std::uniform_int_distribution<> sleep_dist(10, 500);

    std::string messages[] = {
        "PING!", "LOAD TEST ACTIVE", "1000 BOTS ONLINE",
        "your server still alive?", "crypto holding strong",
        "AES-256 + HMAC = ", "this is encrypted spam", "beep boop"
    };

    while (running) {
        std::string msg = messages[gen() % 8] + " [" + std::to_string(id) + "]";
        ets::encode_message(ets::MessageType::Chat, msg, crypto, frame);
        if (send(fd, frame.data(), frame.size(), 0) <= 0) break;

        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_dist(gen)));
    }

    // DISC
    ets::encode_message(ets::MessageType::Disc, "", crypto, frame);
    send(fd, frame.data(), frame.size(), 0);
    close(fd);
}

int main() {
    const std::string secret = "13481232871nqdwrqwf141241e1b2dkw1d1r1uwbdk12481y412r1wjbd1e9uw1d12rr1421kjb1ed1";
    const int NUM_CLIENTS = 1000;

    std::cout << "Launching " << NUM_CLIENTS << " encrypted spam bots...\n";
    std::cout << "Press Ctrl+C to stop\n";

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        threads.emplace_back(client_thread, i + 1, secret);
        if (i % 100 == 99) std::cout << "Spawned " << i + 1 << " bots...\n";
    }

    std::cout << "All " << NUM_CLIENTS << " bots running! Spamming 'general' room...\n";
    std::cout << "Watch your server terminal — it should handle this like a champ.\n";

    // Run for 30 seconds then stop
    std::this_thread::sleep_for(std::chrono::seconds(30));
    running = false;

    for (auto& t : threads) t.join();
    std::cout << "Load test complete.\n";
    return 0;
}