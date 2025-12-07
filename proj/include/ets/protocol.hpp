#pragma once
#include "crypto_context.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <arpa/inet.h>

namespace ets {

enum class MessageType : std::uint8_t {
    Hello = 0,
    Chat  = 1,
    Join  = 2,
    Disc  = 3,
    RoomN = 4,
    UserN = 5
};

struct FrameHeader {
    std::uint8_t  version;   
    std::uint8_t  type;      // MessageType as uint8_t
    std::uint16_t reserved;  // align + future use
    std::uint32_t length;    // length of encrypted blob in bytes
};

constexpr std::uint8_t  PROTOCOL_VERSION      = 1;
constexpr std::size_t   FRAME_HEADER_SIZE     = 8;         // 1+1+2+4
constexpr std::uint32_t MAX_ENCRYPTED_PAYLOAD = 64 * 1024; // 64 KB

/// Serialize header into a raw buffer (network byte order).
inline void write_header(const FrameHeader& hdr, std::uint8_t* out) {

    out[0] = hdr.version;
    out[1] = hdr.type;

    const std::uint16_t res_be = htons(hdr.reserved);
    const std::uint32_t len_be = htonl(hdr.length);

    std::memcpy(out + 2, &res_be, sizeof(res_be));
    std::memcpy(out + 4, &len_be, sizeof(len_be));
}

/// Parse header from raw buffer (network byte order).
inline bool read_header(const std::uint8_t* data, std::size_t len, FrameHeader& out) {
    if (len < FRAME_HEADER_SIZE) return false;

    out.version = data[0];
    out.type    = data[1];

    std::uint16_t res_be = 0;
    std::uint32_t len_be = 0;

    std::memcpy(&res_be, data + 2, sizeof(res_be));
    std::memcpy(&len_be, data + 4, sizeof(len_be));

    out.reserved = ntohs(res_be);
    out.length   = ntohl(len_be);

    return true;
}

/// High-level: takes plaintext and wraps it into:
/// [FrameHeader][ encrypted(seqno+iv+ciphertext+tag) ]
inline bool encode_message(MessageType type,
                           std::string_view plaintext,
                           CryptoContext& crypto,
                           std::vector<std::uint8_t>& out_frame)
{
    //   [ seqno(8) || iv(16) || ciphertext || hmac(32) ]
    std::vector<std::uint8_t> enc_blob;
    if (!crypto.encrypt_and_mac(plaintext, enc_blob)) return false;

    if (enc_blob.size() > MAX_ENCRYPTED_PAYLOAD) return false;

    FrameHeader hdr{};
    hdr.version  = PROTOCOL_VERSION;
    hdr.type     = static_cast<std::uint8_t>(type);
    hdr.reserved = 0;
    hdr.length   = static_cast<std::uint32_t>(enc_blob.size());

    out_frame.clear();
    out_frame.resize(FRAME_HEADER_SIZE + enc_blob.size());

    write_header(hdr, out_frame.data());
    std::memcpy(out_frame.data() + FRAME_HEADER_SIZE, enc_blob.data(), enc_blob.size());

    return true;
}

/// - parsed header
/// - decrypted plaintext
inline bool decode_message(const std::uint8_t* data,
                           std::size_t len,
                           CryptoContext& crypto,
                           FrameHeader& header_out,
                           std::string& plaintext_out)
{
    if (!read_header(data, len, header_out)) return false;

    if (header_out.version != PROTOCOL_VERSION) return false;

    if (header_out.length > MAX_ENCRYPTED_PAYLOAD) return false;

    const std::size_t total_needed = FRAME_HEADER_SIZE + static_cast<std::size_t>(header_out.length);
    if (len < total_needed) return false; // caller hasn't provided full frame yet

    const std::uint8_t* enc_ptr = data + FRAME_HEADER_SIZE;
    const std::size_t   enc_len = static_cast<std::size_t>(header_out.length);

    return crypto.decrypt_and_verify(enc_ptr, enc_len, plaintext_out);
}

} 