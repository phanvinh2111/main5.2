#include "Protocol/SimpleModulus.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>

#include "Protocol/Framing.h"

// Implementation notes
// --------------------
//
// "New" variant: an 8-byte plaintext block expands to an 11-byte
// cipher block.  In addition every encrypted packet starts with an
// extra "counter" byte that the server uses to detect dropped /
// reordered packets.
//
// One block consists of four 18-bit "result" values that are bit-
// packed into 9 bytes (4 * 18 bits = 72 bits = 9 bytes), followed by
// two trailer bytes:
//
//   * Byte 9  — block size encoded as `size XOR BlockCheckSumXorKey
//               XOR checksum XOR BlockSizeXorKey`.
//   * Byte 10 — block checksum, defined as
//               `BlockCheckSumXorKey XOR all_input_bytes`.
//
// Encryption per block:
//
//   r[0] = (xor_key[0] ^ input[0]) * enc_key[0] % mod_key[0]
//   r[i] = (xor_key[i] ^ (input[i] ^ (r[i-1] & 0xFFFF))) * enc_key[i] % mod_key[i]
//   for i in 0..2: r[i] = r[i] ^ xor_key[i] ^ (r[i+1] & 0xFFFF)
//
// Decryption per block reverses this exactly; see OpenMU's
// `PipelinedSimpleModulusDecryptor.DecryptContent`.
//
// The bit packing layout matches OpenMU's WriteResultToTarget /
// ReadInputBuffer helpers byte-for-byte.

namespace mu::proto {

namespace {

constexpr int kDecryptedBlockSize = 8;
constexpr int kEncryptedBlockSize = 11;
constexpr std::uint8_t kBlockSizeXorKey = 0x3D;
constexpr std::uint8_t kBlockCheckSumXorKey = 0xF8;

constexpr int bit_index(int result_index) noexcept {
    return result_index * 18;
}
constexpr int byte_offset(int result_index) noexcept {
    return bit_index(result_index) / 8;
}
constexpr int bit_offset(int result_index) noexcept {
    return bit_index(result_index) % 8;
}
constexpr int first_bit_mask(int result_index) noexcept {
    return 0xFF >> bit_offset(result_index);
}
constexpr int remainder_bit_mask(int result_index) noexcept {
    int b = bit_offset(result_index);
    int hi = (0xFF << (6 - b)) & 0xFF;
    int lo = (0xFF << (8 - b)) & 0xFF;
    return hi - lo;
}

inline std::uint16_t le_u16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

inline void put_le_u16(std::uint8_t* p, std::uint16_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v & 0xFF);
    p[1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
}

inline std::uint32_t byte_swap_u32(std::uint32_t v) noexcept {
    return ((v & 0x000000FFu) << 24)
         | ((v & 0x0000FF00u) << 8)
         | ((v & 0x00FF0000u) >> 8)
         | ((v & 0xFF000000u) >> 24);
}

void write_result_to_target(std::uint8_t* target, int result_index,
                            std::uint32_t result) {
    const int off0 = byte_offset(result_index);
    const int b = bit_offset(result_index);
    const std::uint32_t first_mask =
        static_cast<std::uint32_t>(first_bit_mask(result_index));
    const std::uint32_t swapped = byte_swap_u32(result);

    target[off0] = static_cast<std::uint8_t>(
        target[off0] | ((swapped >> (24 + b)) & first_mask));
    target[off0 + 1] = static_cast<std::uint8_t>(swapped >> (16 + b));
    const std::uint32_t mid_mask =
        (static_cast<std::uint32_t>(0xFFu) << (8 - b)) & 0xFFu;
    target[off0 + 2] = static_cast<std::uint8_t>(
        (swapped >> (8 + b)) & mid_mask);

    const std::uint32_t rem_mask =
        static_cast<std::uint32_t>(remainder_bit_mask(result_index));
    const std::uint32_t remainder = (result >> 16) << (6 - b);
    target[off0 + 2] = static_cast<std::uint8_t>(
        target[off0 + 2] | (remainder & rem_mask));
}

std::uint32_t read_input_buffer(const std::uint8_t* buf, int result_index) {
    const int off0 = byte_offset(result_index);
    const int b = bit_offset(result_index);
    const std::uint32_t first_mask =
        static_cast<std::uint32_t>(first_bit_mask(result_index));
    const std::uint32_t mid_mask =
        (static_cast<std::uint32_t>(0xFFu) << (8 - b)) & 0xFFu;

    // Carefully cast to uint32 BEFORE shifting so we never invoke the
    // signed-overflow undefined behaviour that the previous version
    // tripped on 0x80-or-higher bytes shifted by 24 bits.
    std::uint32_t result = 0;
    result += (static_cast<std::uint32_t>(buf[off0]) & first_mask) << (24 + b);
    result += static_cast<std::uint32_t>(buf[off0 + 1]) << (16 + b);
    result += (static_cast<std::uint32_t>(buf[off0 + 2]) & mid_mask)
              << (8 + b);

    result = byte_swap_u32(result);
    const std::uint32_t rem_mask =
        static_cast<std::uint32_t>(remainder_bit_mask(result_index));
    const std::uint32_t remainder =
        static_cast<std::uint32_t>(buf[off0 + 2]) & rem_mask;
    result += (remainder << 16) >> (6 - b);
    return result;
}

void load_encrypt_key(const std::uint32_t k[12],
                      std::uint32_t mod[4], std::uint32_t enc[4],
                      std::uint32_t xr[4]) {
    for (int i = 0; i < 4; ++i) {
        mod[i] = k[i];
        enc[i] = k[i + 4];
        xr[i]  = k[i + 8];
    }
}

void load_decrypt_key(const std::uint32_t k[12],
                      std::uint32_t mod[4], std::uint32_t dec[4],
                      std::uint32_t xr[4]) {
    for (int i = 0; i < 4; ++i) {
        mod[i] = k[i];
        dec[i] = k[i + 4];
        xr[i]  = k[i + 8];
    }
}

// Encrypt one 8-byte plaintext block (containing `block_size` real
// bytes, the rest zero-padded) into an 11-byte cipher block.
void encrypt_block(const std::uint8_t input[8],
                   std::uint8_t output[11], int block_size,
                   const std::uint32_t mod[4], const std::uint32_t enc[4],
                   const std::uint32_t xr[4]) {
    std::memset(output, 0, 11);

    std::uint32_t r[4];
    std::uint16_t w0 = le_u16(input + 0);
    std::uint16_t w1 = le_u16(input + 2);
    std::uint16_t w2 = le_u16(input + 4);
    std::uint16_t w3 = le_u16(input + 6);
    std::uint16_t w[4] = {w0, w1, w2, w3};

    r[0] = ((xr[0] ^ w[0]) * enc[0]) % mod[0];
    for (int i = 1; i < 4; ++i) {
        r[i] = ((xr[i] ^ (w[i] ^ (r[i - 1] & 0xFFFF))) * enc[i]) % mod[i];
    }
    for (int i = 0; i < 3; ++i) {
        r[i] = r[i] ^ xr[i] ^ (r[i + 1] & 0xFFFF);
    }

    for (int i = 0; i < 4; ++i) {
        write_result_to_target(output, i, r[i]);
    }

    std::uint8_t checksum = kBlockCheckSumXorKey;
    for (int i = 0; i < block_size; ++i) {
        checksum = static_cast<std::uint8_t>(checksum ^ input[i]);
    }
    std::uint8_t encoded_size = static_cast<std::uint8_t>(
        (block_size ^ kBlockSizeXorKey) ^ checksum);
    output[9]  = encoded_size;
    output[10] = checksum;
}

// Decrypt one 11-byte cipher block.  Returns the block size in bytes
// (between 0 and 8 inclusive) or throws on a malformed block.
int decrypt_block(const std::uint8_t input[11], std::uint8_t output[8],
                  const std::uint32_t mod[4], const std::uint32_t dec[4],
                  const std::uint32_t xr[4], bool accept_wrong_checksum) {
    std::uint32_t r[4];
    for (int i = 0; i < 4; ++i) {
        r[i] = read_input_buffer(input, i);
    }
    for (int i = 3; i > 0; --i) {
        r[i - 1] = r[i - 1] ^ xr[i - 1] ^ (r[i] & 0xFFFF);
    }
    std::uint16_t out[4]{};
    for (int i = 0; i < 4; ++i) {
        std::uint32_t result = xr[i] ^ ((r[i] * dec[i]) % mod[i]);
        if (i > 0) result ^= (r[i - 1] & 0xFFFF);
        out[i] = static_cast<std::uint16_t>(result & 0xFFFF);
    }
    for (int i = 0; i < 4; ++i) {
        put_le_u16(output + i * 2, out[i]);
    }

    std::uint8_t encoded_size = input[9];
    std::uint8_t encoded_sum  = input[10];
    int block_size = (encoded_size ^ encoded_sum ^ kBlockSizeXorKey) & 0xFF;
    if (block_size > kDecryptedBlockSize) {
        throw std::runtime_error(
            "SimpleModulus: decoded block size > 8");
    }

    std::uint8_t checksum = kBlockCheckSumXorKey;
    for (int i = 0; i < kDecryptedBlockSize; ++i) {
        checksum = static_cast<std::uint8_t>(checksum ^ output[i]);
    }
    if (checksum != encoded_sum && !accept_wrong_checksum) {
        throw std::runtime_error("SimpleModulus: block checksum mismatch");
    }
    return block_size;
}

int content_size_decrypted(const std::uint8_t* packet) noexcept {
    int header = header_size_for_prefix(packet[0]);
    int total = read_packet_size(packet, 3);
    // On the encrypt side OpenMU adds 1 for the counter byte; the
    // "content" we're sizing is the pre-encryption payload that needs
    // to be split into 8-byte blocks (the counter sits in the first
    // block).
    return (total - header) + 1;
}

int encrypted_size(const std::uint8_t* packet) noexcept {
    int header = header_size_for_prefix(packet[0]);
    int content = content_size_decrypted(packet);
    int blocks = (content + kDecryptedBlockSize - 1) / kDecryptedBlockSize;
    return header + blocks * kEncryptedBlockSize;
}

}  // namespace

SimpleModulusEncryptor::SimpleModulusEncryptor() noexcept {
    load_encrypt_key(keys::kSimpleModulusClientEncrypt.data(),
                     mod_, enc_, xor_);
}

SimpleModulusEncryptor::SimpleModulusEncryptor(
    const std::uint32_t key[12]) noexcept {
    load_encrypt_key(key, mod_, enc_, xor_);
}

std::size_t SimpleModulusEncryptor::worst_case_size(
    const std::uint8_t* packet, std::size_t len) noexcept {
    if (len < 2) return len;
    if (!is_encrypted_prefix(packet[0])) return len;
    return static_cast<std::size_t>(encrypted_size(packet));
}

std::size_t SimpleModulusEncryptor::encrypt_into(
    const std::uint8_t* packet, std::size_t len,
    std::uint8_t* out, std::size_t out_cap) {
    if (len < 2 || out_cap < len) return 0;

    if (!is_encrypted_prefix(packet[0])) {
        std::memcpy(out, packet, len);
        return len;
    }

    int header = header_size_for_prefix(packet[0]);
    std::size_t total_decrypted = len - static_cast<std::size_t>(header);
    std::size_t out_size = static_cast<std::size_t>(encrypted_size(packet));
    if (out_cap < out_size) return 0;

    out[0] = packet[0];
    // header bytes 1..(header-1) will be overwritten by write_packet_size.

    std::size_t src_off = 0;
    std::size_t dst_off = static_cast<std::size_t>(header);
    std::uint8_t input_buffer[kDecryptedBlockSize]{};

    // First block carries the counter as input_buffer[0].
    input_buffer[0] = counter_;
    std::size_t first_take = std::min<std::size_t>(
        total_decrypted, kDecryptedBlockSize - 1);
    std::memcpy(input_buffer + 1, packet + header, first_take);
    std::memset(input_buffer + 1 + first_take, 0,
                kDecryptedBlockSize - 1 - first_take);
    int first_block_size = static_cast<int>(
        std::min<std::size_t>(kDecryptedBlockSize, total_decrypted + 1));
    encrypt_block(input_buffer, out + dst_off, first_block_size,
                  mod_, enc_, xor_);
    dst_off += kEncryptedBlockSize;
    src_off += kDecryptedBlockSize - 1;

    while (src_off < total_decrypted) {
        std::size_t take = std::min<std::size_t>(
            kDecryptedBlockSize, total_decrypted - src_off);
        std::memcpy(input_buffer, packet + header + src_off, take);
        std::memset(input_buffer + take, 0, kDecryptedBlockSize - take);
        encrypt_block(input_buffer, out + dst_off,
                      static_cast<int>(take), mod_, enc_, xor_);
        src_off += kDecryptedBlockSize;
        dst_off += kEncryptedBlockSize;
    }

    counter_ = static_cast<std::uint8_t>((counter_ + 1) & 0xFF);

    write_packet_size(out, dst_off);
    return dst_off;
}

std::vector<std::uint8_t> SimpleModulusEncryptor::encrypt(
    const std::uint8_t* packet, std::size_t len) {
    if (len < 2 || !is_encrypted_prefix(packet[0])) {
        return std::vector<std::uint8_t>(packet, packet + len);
    }
    std::size_t cap = static_cast<std::size_t>(encrypted_size(packet));
    std::vector<std::uint8_t> out(cap);
    std::size_t actual = encrypt_into(packet, len, out.data(), out.size());
    out.resize(actual);
    return out;
}

SimpleModulusDecryptor::SimpleModulusDecryptor() noexcept {
    load_decrypt_key(keys::kSimpleModulusClientDecrypt.data(),
                     mod_, dec_, xor_);
}

SimpleModulusDecryptor::SimpleModulusDecryptor(
    const std::uint32_t key[12]) noexcept {
    load_decrypt_key(key, mod_, dec_, xor_);
}

std::vector<std::uint8_t> SimpleModulusDecryptor::decrypt(
    const std::uint8_t* packet, std::size_t len) {
    if (len < 2) {
        return std::vector<std::uint8_t>(packet, packet + len);
    }
    int header = header_size_for_prefix(packet[0]);
    if (header == 0) {
        return std::vector<std::uint8_t>(packet, packet + len);
    }
    if (!is_encrypted_prefix(packet[0])) {
        return std::vector<std::uint8_t>(packet, packet + len);
    }
    int total = read_packet_size(packet, len);
    if (total <= header) {
        throw std::runtime_error(
            "SimpleModulus: encrypted packet shorter than header");
    }
    int content = total - header;
    if (content % kEncryptedBlockSize != 0) {
        throw std::runtime_error(
            "SimpleModulus: encrypted content not a multiple of 11");
    }
    int blocks = content / kEncryptedBlockSize;

    // Result layout: [prefix][len...][counter][plaintext...].  We
    // write the plaintext and counter from byte `header` onward, then
    // strip the counter to produce the final packet.
    std::vector<std::uint8_t> work;
    work.resize(static_cast<std::size_t>(header) +
                static_cast<std::size_t>(blocks) * kDecryptedBlockSize);
    work[0] = packet[0];

    std::size_t dst = static_cast<std::size_t>(header);
    std::uint8_t out_block[kDecryptedBlockSize];
    for (int b = 0; b < blocks; ++b) {
        const std::uint8_t* in_block =
            packet + header + b * kEncryptedBlockSize;
        int bs = decrypt_block(in_block, out_block,
                               mod_, dec_, xor_, accept_wrong_checksum_);
        if (b == 0) {
            // The first block must contain at least the counter byte.
            // A malformed packet could yield `bs == 0`; without this
            // guard the `bs - 1` arithmetic below would wrap to
            // `SIZE_MAX` and corrupt memory inside `memcpy`.
            if (bs < 1) {
                throw std::runtime_error(
                    "SimpleModulus: first block has size 0 "
                    "(missing counter byte)");
            }
            // Counter check: first block's first byte must equal the
            // expected counter value.
            if (out_block[0] != counter_) {
                throw std::runtime_error(
                    std::string("SimpleModulus: counter mismatch "
                                "(expected ") +
                    std::to_string(static_cast<int>(counter_)) +
                    ", got " +
                    std::to_string(static_cast<int>(out_block[0])) + ")");
            }
            // emit bytes 1..bs from this block (the counter is dropped)
            std::memcpy(work.data() + dst, out_block + 1, bs - 1);
            dst += static_cast<std::size_t>(bs - 1);
        } else {
            std::memcpy(work.data() + dst, out_block, bs);
            dst += static_cast<std::size_t>(bs);
        }
    }
    work.resize(dst);
    write_packet_size(work.data(), work.size());
    counter_ = static_cast<std::uint8_t>((counter_ + 1) & 0xFF);
    return work;
}

}  // namespace mu::proto
