#include "client/src/client.hpp"
#include <iostream>

namespace ui {
  constexpr const char* RESET  = "\033[0m";
  constexpr const char* BOLD   = "\033[1m";
  constexpr const char* CYAN   = "\033[36m";
  constexpr const char* GREEN  = "\033[32m";
  constexpr const char* YELLOW = "\033[33m";
  constexpr const char* BLUE   = "\033[34m";
}

int main() {
  ets_client::Client client;

  // === CONFIGURATION ===
  const std::string shared_secret =
      "13481232871nqdwrqwf141241e1b2dkw1d1r1uwbdk12481y412r1wjbd1e9uw1d12rr1421kjb1ed1";

  client.set_shared_secret(shared_secret);

  std::cout << ui::CYAN << "\n========================================\n"
            << ui::BOLD  << "      Encrypted Chatroom Client\n"
            << ui::RESET << ui::CYAN
            << "========================================\n\n"
            << ui::RESET;

  std::cout << ui::YELLOW << "Connecting to 127.0.0.1:12345 ..." << ui::RESET << std::endl;

  if (!client.connect_to("127.0.0.1", 12345)) {
    std::cerr << ui::YELLOW << "[!] " << ui::RESET
              << "Failed to connect to server. Is the server running?\n";
    return 1;
  }

  std::cout << ui::GREEN << "[✓] " << ui::RESET
            << "Connected! You are now in the encrypted chat.\n\n";

  std::cout << ui::BOLD << "Available Commands:" << ui::RESET << "\n";
  std::cout << "  " << ui::BLUE << "HELLO <name>" << ui::RESET << "   → set your username\n";
  std::cout << "  " << ui::BLUE << "JOIN <room>" << ui::RESET << "    → join or create a room\n";
  std::cout << "  " << ui::BLUE << "/rooms" << ui::RESET << "          → list all rooms\n";
  std::cout << "  " << ui::BLUE << "/users" << ui::RESET << "          → list users in current room\n";
  std::cout << "  " << ui::BLUE << "DISC" << ui::RESET << " or "
            << ui::BLUE << "/quit" << ui::RESET << " → disconnect\n\n";

  std::cout << ui::CYAN
            << "----------------------------------------\n"
            << ui::RESET
            << "Type your message and press Enter to chat.\n"
            << "----------------------------------------\n\n";

  // This blocks until you quit or server disconnects
  return client.run();
}
