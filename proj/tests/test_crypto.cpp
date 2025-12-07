#include "include/ets/crypto_context.hpp"
#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <iomanip>
#include <random>  // For generating large random data

namespace ets {

// Helper function to print bytes in hex
void print_hex(const std::string& label, const std::vector<std::uint8_t>& data) {
    std::cout << label << ": ";
    for (const auto& byte : data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(byte);
    }
    std::cout << std::dec << std::endl;
}

// Test 1: Basic encrypt/decrypt roundtrip
void test_encrypt_decrypt_roundtrip() {
    std::cout << "\n=== Test 1: Basic Encrypt/Decrypt Roundtrip ===" << std::endl;
    const std::string shared_secret = "test_shared_secret_32bytes_or_more";

    CryptoContext sender = CryptoContext::from_shared_secret(shared_secret);
    CryptoContext receiver = CryptoContext::from_shared_secret(shared_secret);

    const std::string plaintext = "Hello, World! This is a secret message.";
    std::vector<std::uint8_t> frame;

    bool enc_ok = sender.encrypt_and_mac(plaintext, frame);
    assert(enc_ok && "Encryption failed");

    std::string decrypted;
    bool dec_ok = receiver.decrypt_and_verify(frame.data(), frame.size(), decrypted);
    assert(dec_ok && "Decryption failed");

    assert(decrypted == plaintext && "Plaintext mismatch");
    std::cout << "✓ Test 1 PASSED" << std::endl;
}

// Test 2: Multiple messages with sequence numbers (in order)
void test_sequence_numbers_in_order() {
    std::cout << "\n=== Test 2: Sequence Numbers (In Order) ===" << std::endl;
    
    std::string secret = "another_test_secret_key";
    CryptoContext sender = CryptoContext::from_shared_secret(secret);
    CryptoContext receiver = CryptoContext::from_shared_secret(secret);
    
    std::vector<std::string> messages = {
        "First message",
        "Second message",
        "Third message"
    };
    
    std::vector<std::vector<std::uint8_t>> encrypted_frames;
    
    // Encrypt multiple messages
    for (const auto& msg : messages) {
        std::vector<std::uint8_t> frame;
        bool result = sender.encrypt_and_mac(msg, frame);
        assert(result && "Encryption failed");
        encrypted_frames.push_back(frame);
        std::cout << "Encrypted message: " << msg << " (frame size: " << frame.size() << ")" << std::endl;
    }
    
    // Decrypt all messages in order
    for (size_t i = 0; i < encrypted_frames.size(); ++i) {
        std::string decrypted;
        bool result = receiver.decrypt_and_verify(encrypted_frames[i].data(), 
                                                  encrypted_frames[i].size(), 
                                                  decrypted);
        assert(result && "Decryption failed");
        assert(decrypted == messages[i] && "Message mismatch");
        std::cout << "Decrypted message " << (i + 1) << ": " << decrypted << std::endl;
    }
    
    std::cout << "✓ Test 2 PASSED" << std::endl;
}

// Test 3: Sequence numbers out of order (should succeed since code allows skipping)
void test_sequence_numbers_out_of_order() {
    std::cout << "\n=== Test 3: Sequence Numbers (Out of Order) ===" << std::endl;
    
    std::string secret = "out_of_order_secret";
    CryptoContext sender = CryptoContext::from_shared_secret(secret);
    CryptoContext receiver = CryptoContext::from_shared_secret(secret);
    
    std::vector<std::string> messages = {
        "Message 0",
        "Message 1",
        "Message 2"
    };
    
    std::vector<std::vector<std::uint8_t>> encrypted_frames;
    
    // Encrypt
    for (const auto& msg : messages) {
        std::vector<std::uint8_t> frame;
        bool result = sender.encrypt_and_mac(msg, frame);
        assert(result && "Encryption failed");
        encrypted_frames.push_back(frame);
    }
    
    // Decrypt out of order: 1, 2, 0 (0 should fail after 1 and 2)
    std::vector<size_t> order = {1, 2, 0};
    std::vector<bool> expected_success = {true, true, false};
    
    for (size_t idx = 0; idx < order.size(); ++idx) {
        size_t msg_idx = order[idx];
        std::string decrypted;
        bool result = receiver.decrypt_and_verify(encrypted_frames[msg_idx].data(), 
                                                  encrypted_frames[msg_idx].size(), 
                                                  decrypted);
        assert(result == expected_success[idx] && "Unexpected decryption result");
        if (result) {
            assert(decrypted == messages[msg_idx] && "Message mismatch");
            std::cout << "Decrypted message " << msg_idx << ": " << decrypted << std::endl;
        } else {
            std::cout << "Correctly rejected out-of-order/old message " << msg_idx << std::endl;
        }
    }
    
    std::cout << "✓ Test 3 PASSED" << std::endl;
}

// Test 4: Replay attack detection
void test_replay_attack() {
    std::cout << "\n=== Test 4: Replay Attack Detection ===" << std::endl;
    
    std::string secret = "replay_test_secret";
    CryptoContext sender = CryptoContext::from_shared_secret(secret);
    CryptoContext receiver = CryptoContext::from_shared_secret(secret);
    
    std::string plaintext = "This is a message";
    std::vector<std::uint8_t> frame;
    
    bool enc_result = sender.encrypt_and_mac(plaintext, frame);
    assert(enc_result && "Encryption failed");
    
    // Decrypt first time
    std::string decrypted1;
    bool dec1 = receiver.decrypt_and_verify(frame.data(), frame.size(), decrypted1);
    assert(dec1 && "First decryption failed");
    assert(decrypted1 == plaintext && "Message mismatch");
    
    // Try replay
    std::string decrypted2;
    bool dec2 = receiver.decrypt_and_verify(frame.data(), frame.size(), decrypted2);
    assert(!dec2 && "Replay should have been detected");
    
    std::cout << "✓ Test 4 PASSED" << std::endl;
}

// Test 5: Detect tampered data (ciphertext)
void test_tampered_ciphertext() {
    std::cout << "\n=== Test 5: Tampered Ciphertext Detection ===" << std::endl;
    
    std::string secret = "tamper_test_secret";
    CryptoContext sender = CryptoContext::from_shared_secret(secret);
    CryptoContext receiver = CryptoContext::from_shared_secret(secret);
    
    std::string plaintext = "This is important data";
    std::vector<std::uint8_t> encrypted;
    
    bool enc_result = sender.encrypt_and_mac(plaintext, encrypted);
    assert(enc_result && "Encryption failed");
    std::cout << "Original encrypted frame size: " << encrypted.size() << std::endl;
    
    // Tamper with ciphertext (after nonce + IV = 24 bytes)
    if (encrypted.size() > 40) {
        std::cout << "Tampering with byte at position 40..." << std::endl;
        encrypted[40] ^= 0xFF;
    }
    
    // Try to decrypt
    std::string decrypted;
    bool dec_result = receiver.decrypt_and_verify(encrypted.data(), encrypted.size(), decrypted);
    assert(!dec_result && "Tampered data should have been rejected");
    
    std::cout << "✓ Test 5 PASSED" << std::endl;
}

// Test 6: Detect tampered tag
void test_tampered_tag() {
    std::cout << "\n=== Test 6: Tampered Tag Detection ===" << std::endl;
    
    std::string secret = "tag_tamper_secret";
    CryptoContext sender = CryptoContext::from_shared_secret(secret);
    CryptoContext receiver = CryptoContext::from_shared_secret(secret);
    
    std::string plaintext = "Data with tag";
    std::vector<std::uint8_t> encrypted;
    
    bool enc_result = sender.encrypt_and_mac(plaintext, encrypted);
    assert(enc_result && "Encryption failed");
    
    // Tamper with tag (last 32 bytes)
    size_t tag_start = encrypted.size() - TAG_SIZE;
    encrypted[tag_start] ^= 0xFF;
    
    std::string decrypted;
    bool dec_result = receiver.decrypt_and_verify(encrypted.data(), encrypted.size(), decrypted);
    assert(!dec_result && "Tampered tag should have been rejected");
    
    std::cout << "✓ Test 6 PASSED" << std::endl;
}

// Test 7: Detect tampered sequence number
void test_tampered_seqno() {
    std::cout << "\n=== Test 7: Tampered Sequence Number Detection ===" << std::endl;
    
    std::string secret = "seq_tamper_secret";
    CryptoContext sender = CryptoContext::from_shared_secret(secret);
    CryptoContext receiver = CryptoContext::from_shared_secret(secret);
    
    std::string plaintext = "Sequence protected data";
    std::vector<std::uint8_t> encrypted;
    
    bool enc_result = sender.encrypt_and_mac(plaintext, encrypted);
    assert(enc_result && "Encryption failed");
    
    // Tamper with seqno (first 8 bytes)
    encrypted[0] ^= 0xFF;
    
    std::string decrypted;
    bool dec_result = receiver.decrypt_and_verify(encrypted.data(), encrypted.size(), decrypted);
    assert(!dec_result && "Tampered seqno should have been rejected (via HMAC)");
    
    std::cout << "✓ Test 7 PASSED" << std::endl;
}

// Test 8: Invalid short frame
void test_short_frame() {
    std::cout << "\n=== Test 8: Short Frame Rejection ===" << std::endl;
    
    std::string secret = "short_frame_secret";
    CryptoContext receiver = CryptoContext::from_shared_secret(secret);
    
    std::vector<std::uint8_t> short_frame(20);  // Less than min size
    std::string decrypted;
    bool dec_result = receiver.decrypt_and_verify(short_frame.data(), short_frame.size(), decrypted);
    assert(!dec_result && "Short frame should have been rejected");
    
    std::cout << "✓ Test 8 PASSED" << std::endl;
}

// Test 9: Empty message
void test_empty_message() {
    std::cout << "\n=== Test 9: Empty Message ===" << std::endl;
    
    std::string secret = "empty_test_secret";
    CryptoContext sender = CryptoContext::from_shared_secret(secret);
    CryptoContext receiver = CryptoContext::from_shared_secret(secret);
    
    std::string plaintext = "";
    std::vector<std::uint8_t> encrypted;
    
    bool enc_result = sender.encrypt_and_mac(plaintext, encrypted);
    assert(enc_result && "Encryption of empty message failed");
    
    std::string decrypted;
    bool dec_result = receiver.decrypt_and_verify(encrypted.data(), encrypted.size(), decrypted);
    assert(dec_result && "Decryption of empty message failed");
    assert(decrypted == plaintext && "Empty message mismatch");
    
    std::cout << "✓ Test 9 PASSED" << std::endl;
}

// Test 10: Different secrets produce different keys
void test_different_secrets() {
    std::cout << "\n=== Test 10: Different Secrets ===" << std::endl;
    
    std::string secret1 = "secret_one";
    std::string secret2 = "secret_two";
    
    Key key1_enc{}, key1_mac{};
    Key key2_enc{}, key2_mac{};
    
    CryptoContext::kdf_from_shared_secret(secret1, key1_enc, key1_mac);
    CryptoContext::kdf_from_shared_secret(secret2, key2_enc, key2_mac);
    
    bool keys_differ = (key1_enc != key2_enc) || (key1_mac != key2_mac);
    assert(keys_differ && "Different secrets should produce different keys");
    
    std::cout << "✓ Test 10 PASSED" << std::endl;
}

// Test 11: Same secret produces same keys
void test_same_secret_same_keys() {
    std::cout << "\n=== Test 11: Same Secret Produces Same Keys ===" << std::endl;
    
    std::string secret = "consistent_secret";
    
    Key key1_enc{}, key1_mac{};
    Key key2_enc{}, key2_mac{};
    
    CryptoContext::kdf_from_shared_secret(secret, key1_enc, key1_mac);
    CryptoContext::kdf_from_shared_secret(secret, key2_enc, key2_mac);
    
    assert(key1_enc == key2_enc && "Same secret should produce same encryption keys");
    assert(key1_mac == key2_mac && "Same secret should produce same MAC keys");
    
    std::cout << "✓ Test 11 PASSED" << std::endl;
}

// Test 12: Large message (1MB)
void test_large_message() {
    std::cout << "\n=== Test 12: Large Message ===" << std::endl;
    
    std::string secret = "large_message_secret";
    CryptoContext sender = CryptoContext::from_shared_secret(secret);
    CryptoContext receiver = CryptoContext::from_shared_secret(secret);
    
    // Create a large message (1MB of random data)
    const size_t large_size = 1024 * 1024;
    std::string large_plaintext(large_size, 0);
    std::mt19937 gen(42);  // Fixed seed for reproducibility
    std::uniform_int_distribution<char> dist(0, 255);
    for (char& c : large_plaintext) {
        c = dist(gen);
    }
    std::cout << "Testing with " << large_plaintext.size() << " byte message" << std::endl;
    
    std::vector<std::uint8_t> encrypted;
    bool enc_result = sender.encrypt_and_mac(large_plaintext, encrypted);
    assert(enc_result && "Encryption of large message failed");
    std::cout << "Encrypted size: " << encrypted.size() << " bytes" << std::endl;
    
    std::string decrypted;
    bool dec_result = receiver.decrypt_and_verify(encrypted.data(), encrypted.size(), decrypted);
    assert(dec_result && "Decryption of large message failed");
    assert(decrypted == large_plaintext && "Large message mismatch");
    
    std::cout << "✓ Test 12 PASSED" << std::endl;
}

}  // namespace ets

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n"
              << "║        CryptoContext Test Suite                            ║\n"
              << "╚════════════════════════════════════════════════════════════╝" << std::endl;
    
    try {
        ets::test_encrypt_decrypt_roundtrip();
        ets::test_sequence_numbers_in_order();
        ets::test_sequence_numbers_out_of_order();
        ets::test_replay_attack();
        ets::test_tampered_ciphertext();
        ets::test_tampered_tag();
        ets::test_tampered_seqno();
        ets::test_short_frame();
        ets::test_empty_message();
        ets::test_different_secrets();
        ets::test_same_secret_same_keys();
        ets::test_large_message();
        
        std::cout << "\n╔════════════════════════════════════════════════════════════╗\n"
                  << "║          ✓ All Tests PASSED!                               ║\n"
                  << "╚════════════════════════════════════════════════════════════╝" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}