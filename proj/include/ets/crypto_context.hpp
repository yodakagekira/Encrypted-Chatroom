#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ets {

// Constants for cryptography
static constexpr std::size_t KEY_SIZE = 32;      // AES-256 key size
static constexpr std::size_t IV_SIZE = 16;       // AES block size
static constexpr std::size_t TAG_SIZE = 32;      // HMAC-SHA256 output size
static constexpr std::size_t NONCE_SIZE = 8;     // Sequence number size

// Type aliases
using Key = std::array<std::uint8_t, KEY_SIZE>;
using Iv = std::array<std::uint8_t, IV_SIZE>;

class CryptoContext {
public:
    CryptoContext(const Key& enc_key, const Key& mac_key);

    // Static factory method from shared secret
    static CryptoContext from_shared_secret(std::string_view secret);

    // Key derivation from shared secret
    static void kdf_from_shared_secret(std::string_view secret, Key& enc_key, Key& mac_key);

    // Fill IV with random bytes
    static void fill_random_iv(Iv& iv);

    bool encrypt_and_mac(std::string_view plaintext, std::vector<std::uint8_t>& out_frame);
    bool decrypt_and_verify(const std::uint8_t* frame, std::size_t frame_len, std::string& out_plaintext);

private:
    Key enc_key_;           
    Key mac_key_;           
    std::uint64_t seqno_;   // Sequence number for anti-replay
};

} 