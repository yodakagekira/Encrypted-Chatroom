#include "client/src/client.hpp"
#include <iostream>

int main() {
    ets_client::Client client;

    // === CONFIGURATION ===
    // Change this to match the shared_secret in configs/server.toml
    const std::string shared_secret = "13481232871nqdwrqwf141241e1b2dkw1d1r1uwbdk12481y412r1wjbd1e9uw1d12rr1421kjb1ed1";

    client.set_shared_secret(shared_secret);

    std::cout << "Connecting to 127.0.0.1:12345 ...\n";
    if (!client.connect_to("127.0.0.1", 12345)) {
        std::cerr << "Failed to connect to server. Is the server running?\n";
        return 1;
    }

    std::cout << "Connected! You are in the encrypted chat.\n";
    std::cout << "Commands:\n";
    std::cout << "  HELLO <your name>\n";
    std::cout << "  JOIN <room name>\n";
    std::cout << "  /quit  or  DISC   → leave\n";
    std::cout << "\nJust type and press Enter to chat.\n\n";

    // This blocks until you quit or server disconnects
    return client.run();
}