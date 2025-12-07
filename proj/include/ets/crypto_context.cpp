#include "crypto_context.hpp"
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <cstring>
#include <stdexcept>

namespace ets {

CryptoContext::CryptoContext(const Key& enc_key, const Key& mac_key)
    : enc_key_(enc_key), mac_key_(mac_key), seqno_(0) {}

// Simple HMAC-based KDF (demo)
void CryptoContext::kdf_from_shared_secret(std::string_view secret, Key& enc_key, Key& mac_key) {
    unsigned int len = 0;
    unsigned char tmp[TAG_SIZE];

    HMAC(EVP_sha256(),
         reinterpret_cast<const unsigned char*>(secret.data()),
         static_cast<int>(secret.size()),
         reinterpret_cast<const unsigned char*>("ENC"), 3,
         tmp, &len);
    std::memcpy(enc_key.data(), tmp, KEY_SIZE);

    HMAC(EVP_sha256(),
         reinterpret_cast<const unsigned char*>(secret.data()),
         static_cast<int>(secret.size()),
         reinterpret_cast<const unsigned char*>("MAC"), 3,
         tmp, &len);
    std::memcpy(mac_key.data(), tmp, KEY_SIZE);
}

CryptoContext CryptoContext::from_shared_secret(std::string_view secret) {
    Key enc{}, mac{};
    kdf_from_shared_secret(secret, enc, mac);
    return CryptoContext(enc, mac);
}

void CryptoContext::fill_random_iv(Iv& iv) {
    if (RAND_bytes(iv.data(), static_cast<int>(iv.size())) != 1)
        throw std::runtime_error("RAND_bytes failed");
}

bool CryptoContext::encrypt_and_mac(std::string_view plaintext, std::vector<std::uint8_t>& out_frame) {
    Iv iv;
    try { fill_random_iv(iv); } catch (...) { return false; }

    std::vector<unsigned char> ciphertext(plaintext.size() + EVP_MAX_BLOCK_LENGTH);
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    int out_len1 = 0, out_len2 = 0;
    
    auto init_result = EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, enc_key_.data(), iv.data());
    auto update_result = EVP_EncryptUpdate(ctx, ciphertext.data(), &out_len1, reinterpret_cast<const unsigned char*>(plaintext.data()), static_cast<int>(plaintext.size()));
    auto final_result = EVP_EncryptFinal_ex(ctx, ciphertext.data() + out_len1, &out_len2);

    if (init_result != 1 || update_result != 1 || final_result!= 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    EVP_CIPHER_CTX_free(ctx);
    ciphertext.resize(static_cast<std::size_t>(out_len1 + out_len2));

    // Construct frame: [seqno(8)] [iv(16)] [ciphertext] [tag(32)]
    const std::uint64_t current_seq = seqno_++;
    const std::size_t header_len = NONCE_SIZE + IV_SIZE;
    const std::size_t total_len = header_len + ciphertext.size() + TAG_SIZE;

    out_frame.assign(total_len, 0);
    std::uint8_t* p = out_frame.data();
    std::memcpy(p, &current_seq, sizeof(current_seq)); p += NONCE_SIZE;
    std::memcpy(p, iv.data(), IV_SIZE); p += IV_SIZE;
    std::memcpy(p, ciphertext.data(), ciphertext.size()); p += ciphertext.size();

    // Compute HMAC
    unsigned int tag_len = 0; unsigned char tag_buf[TAG_SIZE];
    HMAC(EVP_sha256(), mac_key_.data(), static_cast<int>(mac_key_.size()),
         out_frame.data(), static_cast<int>(header_len + ciphertext.size()),
         tag_buf, &tag_len);
    if (tag_len < TAG_SIZE) return false;
    std::memcpy(p, tag_buf, TAG_SIZE);
    return true;
}

bool CryptoContext::decrypt_and_verify(const std::uint8_t* frame, std::size_t frame_len, std::string& out_plaintext) {  
    if (frame_len < NONCE_SIZE + IV_SIZE + TAG_SIZE) return false;                      
    const std::size_t header_len = NONCE_SIZE + IV_SIZE;

    if (frame_len < header_len + TAG_SIZE) return false;
    const std::size_t ciphertext_len = frame_len - header_len - TAG_SIZE;
    
    if (ciphertext_len == 0) return false;

    //  Parse frame
    const std::uint8_t* p = frame;                                                      
    std::uint64_t recv_seq = 0; std::memcpy(&recv_seq, p, NONCE_SIZE); p += NONCE_SIZE;     

    Iv iv; std::memcpy(iv.data(), p, IV_SIZE); p += IV_SIZE;
    const unsigned char* ciphertext = p; p += ciphertext_len;
    unsigned char recv_tag[TAG_SIZE]; std::memcpy(recv_tag, p, TAG_SIZE);

    // Verify HMAC
    unsigned char calc_tag[TAG_SIZE]; unsigned int calc_len = 0;
    HMAC(EVP_sha256(), mac_key_.data(), static_cast<int>(mac_key_.size()),
         frame, static_cast<int>(header_len + ciphertext_len), calc_tag, &calc_len);
    if (calc_len < TAG_SIZE) return false;

    unsigned char diff = 0;
    for (std::size_t i = 0; i < TAG_SIZE; ++i) diff |= static_cast<unsigned char>(recv_tag[i] ^ calc_tag[i]);
    if (diff != 0) return false;

    if (recv_seq < seqno_) return false;
    seqno_ = recv_seq + 1;

    // Decrypts
    std::vector<unsigned char> plain(ciphertext_len);
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new(); if (!ctx) return false;
    int out_len1 = 0, out_len2 = 0;
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, enc_key_.data(), iv.data()) != 1 ||

        EVP_DecryptUpdate(ctx, plain.data(), &out_len1, ciphertext, static_cast<int>(ciphertext_len)) != 1 ||

        EVP_DecryptFinal_ex(ctx, plain.data() + out_len1, &out_len2) != 1) {
        EVP_CIPHER_CTX_free(ctx); return false;
    }
    
    // Clean up and assign output
    EVP_CIPHER_CTX_free(ctx);
    plain.resize(static_cast<std::size_t>(out_len1 + out_len2));
    out_plaintext.assign(reinterpret_cast<const char*>(plain.data()), plain.size());
    return true;
}

} 
