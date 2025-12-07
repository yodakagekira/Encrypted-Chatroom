#include "ets_server/server.hpp"
#include "ets_server/server_config.hpp"
#include <iostream>
#include <fstream>

int main(int argc, char* argv[]) {
    try {
        // Load config from configs/server.toml (or fallback to defaults)
        std::string config_path = "configs/server.toml";
        ets_server::ServerConfig cfg;

        if (std::ifstream(config_path)) {
            std::cout << "[server] Loading config from " << config_path << "\n";
            cfg = ets_server::ServerConfig::from_file(config_path);
        } else {
            std::cout << "[server] Config not found, using defaults\n";
            cfg = ets_server::ServerConfig{};  // use defaults
        }

        std::cout << "[server] Starting on " << cfg.bind_address << ":" << cfg.port << "\n";
        std::cout << "[server] Shared secret loaded (length: " << cfg.shared_secret.size() << ")\n";

        ets_server::EpollServer server(std::move(cfg));
        server.run();  // blocks forever

    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}