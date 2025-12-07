#include "ets_server/server.hpp"
#include "ets_server/connection.hpp"
#include "ets_server/chat_hub.hpp"
#include "ets_server/room_manager.hpp"
#include "ets_server/server_config.hpp"   

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace ets_server {

static inline void ltrim(std::string& s) {
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    s.erase(0, i);
}

static inline void rtrim(std::string& s) {
    std::size_t i = s.size();
    while (i > 0 && std::isspace(static_cast<unsigned char>(s[i - 1]))) --i;
    s.erase(i);
}

static inline void trim(std::string& s) {
    ltrim(s);
    rtrim(s);
}

static inline void strip_comment(std::string& s) {
    // TOML uses # for comments; keep it simple
    auto pos = s.find('#');
    if (pos != std::string::npos) s.erase(pos);
}

static inline bool strip_quotes(std::string& s) {
    trim(s);
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        s = s.substr(1, s.size() - 2);
        return true;
    }
    return false;
}

static inline int parse_int(const std::string& raw, const std::string& key) {
    std::string s = raw;
    trim(s);
    if (s.empty()) throw std::runtime_error("Empty int value for key: " + key);

    // Allow underscores? keep strict: digits +/- only.
    std::size_t idx = 0;
    int v = 0;
    try {
        v = std::stoi(s, &idx, 10);
    } catch (...) {
        throw std::runtime_error("Invalid int for key '" + key + "': " + s);
    }
    if (idx != s.size()) {
        throw std::runtime_error("Invalid trailing chars for key '" + key + "': " + s);
    }
    return v;
}

static inline std::string parse_string(const std::string& raw) {
    std::string s = raw;
    trim(s);
    strip_quotes(s); // if not quoted, keep as-is
    return s;
}

ServerConfig ServerConfig::from_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Failed to open config file: " + path);
    }

    ServerConfig cfg{};
    std::string line;
    int lineno = 0;

    while (std::getline(in, line)) {
        ++lineno;

        strip_comment(line);
        trim(line);
        if (line.empty()) continue;

        // Expect: key = value
        auto eq = line.find('=');
        if (eq == std::string::npos) {
            throw std::runtime_error("Config parse error at line " + std::to_string(lineno) +
                                     ": expected 'key = value'");
        }

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        trim(key);
        trim(val);

        if (key == "bind_address") {
            cfg.bind_address = parse_string(val);
            if (cfg.bind_address.empty())
                throw std::runtime_error("bind_address cannot be empty (line " + std::to_string(lineno) + ")");
        } else if (key == "port") {
            cfg.port = parse_int(val, key);
            if (cfg.port <= 0 || cfg.port > 65535)
                throw std::runtime_error("port out of range (1..65535) at line " + std::to_string(lineno));
        } else if (key == "max_connections") {
            cfg.max_connections = parse_int(val, key);
            if (cfg.max_connections < 0)
                throw std::runtime_error("max_connections must be >= 0 at line " + std::to_string(lineno));
        } else if (key == "shared_secret") {
            cfg.shared_secret = parse_string(val);
            if (cfg.shared_secret.empty())
                throw std::runtime_error("shared_secret cannot be empty (line " + std::to_string(lineno) + ")");
        } else {
            // unknown key: ignore (lets you add more later without breaking)
        }
    }

    return cfg;
}

} 
