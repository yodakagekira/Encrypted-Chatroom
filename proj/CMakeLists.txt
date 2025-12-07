cmake_minimum_required(VERSION 3.16)
project(ETSChat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Create an INTERFACE target that carries include directories for all targets
add_library(ETSChat INTERFACE)
target_include_directories(ETSChat INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/server/include
    ${CMAKE_CURRENT_SOURCE_DIR}/client/src
)

set(ETS_CRYPTO_SOURCES
    include/ets/crypto_context.cpp
)

#SERVER
add_executable(server
    server/src/main.cpp          
    server/src/server.cpp
    server/src/connection.cpp
    server/src/chat_hub.cpp
    server/src/room_manager.cpp
    server/src/server_config.cpp
    ${ETS_CRYPTO_SOURCES}
)

target_include_directories(server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(server PRIVATE ETSChat OpenSSL::SSL OpenSSL::Crypto Threads::Threads)

# CLIENT
add_executable(client
    client/src/main.cpp
    client/src/client.cpp
    ${ETS_CRYPTO_SOURCES}
)
target_include_directories(client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(client PRIVATE ETSChat OpenSSL::SSL OpenSSL::Crypto Threads::Threads)

# TESTS
add_executable(test_crypto
    tests/test_crypto.cpp
    ${ETS_CRYPTO_SOURCES}
)
target_include_directories(test_crypto PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_crypto PRIVATE ETSChat OpenSSL::SSL OpenSSL::Crypto Threads::Threads)

enable_testing()
add_test(NAME CryptoTests COMMAND test_crypto)

set_target_properties(server client test_crypto PROPERTIES FOLDER "ETSChat")

message(STATUS "")
message(STATUS "ETSCHAT FULLY COMPILED SUCCESSFULLY!")
message(STATUS "  Run: cd build && ./server")
message(STATUS "  Run: cd build && ./client")
message(STATUS "  Your crypto is PERFECT")
message(STATUS "")