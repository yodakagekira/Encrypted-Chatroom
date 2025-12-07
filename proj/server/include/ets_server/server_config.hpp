#pragma once

#include <string>

namespace ets_server {

struct ServerConfig {
    std::string bind_address = "0.0.0.0";
    int         port = 12345;
    int         max_connections = 1024;

    // Stage 1 key material (later you can replace with KDS-derived per-user keys)
    std::string shared_secret = "13481232871nqdwrqwf141241e1b2dkw1d1r1uwbdk12481y412r1wjbd1e9uw1d12rr1421kjb1ed1";

    static ServerConfig from_file(const std::string& path);
};

} 
