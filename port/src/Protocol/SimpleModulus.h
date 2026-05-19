#pragma once

// "Simple modulus" packet encryption — the MU Online block cipher used
// for C3 / C4 packets.
//
// This is a byte-exact port of MUnique/OpenMU
// `src/Network/SimpleModulus/PipelinedSimpleModulus{Base,Encryptor,Decryptor}.cs`,
// "new" variant: 8-byte plaintext blocks expand to 11-byte cipher
// blocks (4 × 18-bit values + 1 size byte + 1 checksum byte), with a
// 1-byte counter prepended to the first block of every packet.
//
// All routines operate on a single complete framed packet
// (C1/C2/C3/C4) at a time.  C1 and C2 packets are written through
// untouched; only C3 and C4 are transformed, matching OpenMU.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Protocol/Keys.h"

namespace mu::proto {

class SimpleModulusEncryptor {
public:
    // Constructs an encryptor with the OpenMU client-side default key.
    SimpleModulusEncryptor() noexcept;

    // Constructs an encryptor with a custom key (12 uint32 in the
    // `{ mod[4], enc[4], xor[4] }` layout produced by OpenMU's
    // `SimpleModulusKeys.CreateEncryptionKeys`).
    explicit SimpleModulusEncryptor(
        const std::uint32_t key[12]) noexcept;

    // Encrypts a complete framed packet.  C3 / C4 payloads are
    // transformed; C1 / C2 packets are returned verbatim.  The output
    // vector includes the framing header with an updated length field.
    std::vector<std::uint8_t> encrypt(
        const std::uint8_t* packet, std::size_t len);

    // Encrypts in place into `out` using a caller-owned buffer.
    // Returns the new packet length.  The buffer must have room for at
    // least `worst_case_size(len)` bytes.  Returns 0 if `out` is too
    // small.
    std::size_t encrypt_into(const std::uint8_t* packet, std::size_t len,
                             std::uint8_t* out, std::size_t out_cap);

    // Worst-case ciphertext size for a plaintext packet of `len` bytes
    // (with a C3/C4 prefix).  Returns `len` for pass-through prefixes.
    static std::size_t worst_case_size(const std::uint8_t* packet,
                                       std::size_t len) noexcept;

    // Resets the internal block counter.  Useful for tests; the wire
    // protocol expects the counter to monotonically increase across a
    // single connection lifetime.
    void reset() noexcept { counter_ = 0; }

    std::uint8_t counter() const noexcept { return counter_; }

private:
    std::uint32_t mod_[4]{};
    std::uint32_t enc_[4]{};
    std::uint32_t xor_[4]{};
    std::uint8_t counter_{0};
};

class SimpleModulusDecryptor {
public:
    SimpleModulusDecryptor() noexcept;
    explicit SimpleModulusDecryptor(
        const std::uint32_t key[12]) noexcept;

    // Decrypts a complete framed packet.  Throws std::runtime_error
    // when the encrypted content size isn't a multiple of 11, when a
    // block checksum doesn't match, or when the block counter doesn't
    // match the expected value.  Pass-through for C1 / C2.
    std::vector<std::uint8_t> decrypt(
        const std::uint8_t* packet, std::size_t len);

    // If true, the decryptor logs but does not reject blocks whose
    // checksum doesn't match.  Matches OpenMU's
    // `AcceptWrongBlockChecksum`.
    void set_accept_wrong_checksum(bool v) noexcept {
        accept_wrong_checksum_ = v;
    }

    void reset() noexcept { counter_ = 0; }

    std::uint8_t counter() const noexcept { return counter_; }

private:
    std::uint32_t mod_[4]{};
    std::uint32_t dec_[4]{};
    std::uint32_t xor_[4]{};
    std::uint8_t counter_{0};
    bool accept_wrong_checksum_{false};
};

}  // namespace mu::proto
