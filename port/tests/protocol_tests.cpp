// Unit tests for the MU Online protocol codec.
//
// Test vectors are byte-exact ports of MUnique/OpenMU's
// `tests/MUnique.OpenMU.Network.Tests/Pipelined{En,De}cryptorTests.cs`
// — the same packets the upstream server-side suite runs against the
// C# implementation.  Round-trip tests additionally exercise the full
// outbound + inbound pipeline composed by `proto::Connection`.
//
// We deliberately keep the harness dependency-free (no gtest/catch2)
// because the M1 port already builds in pure CMake + SDL3 and the
// snapshot blueprint shouldn't grow a heavyweight test runtime for a
// 10-line codec verifier.  Each TEST() registers itself; main() runs
// them all and reports a count to the CI log.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#include "Network/TcpClient.h"
#include "Protocol/Connection.h"
#include "Protocol/Framing.h"
#include "Protocol/Keys.h"
#include "Protocol/SimpleModulus.h"
#include "Protocol/Xor.h"

namespace {

struct TestCase {
    const char* name;
    std::function<void()> body;
};

std::vector<TestCase>& tests() {
    static std::vector<TestCase> v;
    return v;
}

struct TestRegister {
    TestRegister(const char* name, std::function<void()> body) {
        tests().push_back({name, std::move(body)});
    }
};

#define TEST(name)                                                       \
    static void test_##name();                                           \
    static TestRegister reg_##name(#name, &test_##name);                 \
    static void test_##name()

void fail(const std::string& msg) {
    throw std::runtime_error(msg);
}

template <class A, class B>
void assert_eq(const A& a, const B& b, const char* what) {
    if (!(a == b)) {
        throw std::runtime_error(std::string("expected ") + what);
    }
}

std::string hex_dump(const std::vector<std::uint8_t>& bytes,
                     std::size_t max = 32) {
    std::string s;
    char buf[8];
    for (std::size_t i = 0; i < std::min(bytes.size(), max); ++i) {
        std::snprintf(buf, sizeof(buf), "%02X ", bytes[i]);
        s += buf;
    }
    if (bytes.size() > max) s += "...";
    return s;
}

void assert_bytes_eq(const std::vector<std::uint8_t>& a,
                     const std::vector<std::uint8_t>& b,
                     const char* what) {
    if (a == b) return;
    std::string msg = std::string(what) + " mismatch\n  expected: "
                    + hex_dump(b) + "\n  actual:   " + hex_dump(a);
    throw std::runtime_error(msg);
}

// Decode a base64 string into a byte vector.  Only used to embed the
// reference vectors directly from the OpenMU C# tests.
std::vector<std::uint8_t> b64decode(const std::string& s) {
    static const std::string alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int rev[256];
    for (int i = 0; i < 256; ++i) rev[i] = -1;
    for (int i = 0; i < 64; ++i) rev[(unsigned char)alphabet[i]] = i;

    std::vector<std::uint8_t> out;
    out.reserve(s.size() * 3 / 4);
    int val = 0, bits = 0;
    for (char c : s) {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
        int v = rev[(unsigned char)c];
        if (v < 0) continue;
        val = (val << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((val >> bits) & 0xFF));
        }
    }
    return out;
}

// ---------------------------------------------------------------------
// Framing
// ---------------------------------------------------------------------

TEST(framing_header_sizes) {
    assert_eq(mu::proto::header_size_for_prefix(0xC1), 2, "C1 hdr=2");
    assert_eq(mu::proto::header_size_for_prefix(0xC3), 2, "C3 hdr=2");
    assert_eq(mu::proto::header_size_for_prefix(0xC2), 3, "C2 hdr=3");
    assert_eq(mu::proto::header_size_for_prefix(0xC4), 3, "C4 hdr=3");
    assert_eq(mu::proto::header_size_for_prefix(0x77), 0, "unknown");
}

TEST(framing_read_size) {
    std::vector<std::uint8_t> c1 = {0xC1, 0x05, 0xAA, 0xBB, 0xCC};
    assert_eq(mu::proto::read_packet_size(c1.data(), c1.size()), 5,
              "C1 len=5");

    std::vector<std::uint8_t> c2 = {0xC2, 0x01, 0x23, 0x99};
    assert_eq(mu::proto::read_packet_size(c2.data(), c2.size()),
              0x0123, "C2 len=0x0123");
}

TEST(framing_write_size) {
    std::vector<std::uint8_t> c2 = {0xC2, 0x00, 0x00, 0x99, 0xAA};
    mu::proto::write_packet_size(c2.data(), c2.size());
    assert_eq((int)c2[1], 0x00, "len hi");
    assert_eq((int)c2[2], 0x05, "len lo");
}

// ---------------------------------------------------------------------
// Xor3
// ---------------------------------------------------------------------

TEST(xor3_known_vector) {
    // Known plaintext "Devin01" XOR'd with the default Xor3 key.
    // Hand-computed:
    //   'D'(0x44) ^ 0xFC = 0xB8
    //   'e'(0x65) ^ 0xCF = 0xAA
    //   'v'(0x76) ^ 0xAB = 0xDD
    //   'i'(0x69) ^ 0xFC = 0x95
    //   'n'(0x6E) ^ 0xCF = 0xA1
    //   '0'(0x30) ^ 0xAB = 0x9B
    //   '1'(0x31) ^ 0xFC = 0xCD
    std::vector<std::uint8_t> data = {'D', 'e', 'v', 'i', 'n', '0', '1'};
    std::vector<std::uint8_t> expected = {0xB8, 0xAA, 0xDD, 0x95, 0xA1,
                                          0x9B, 0xCD};
    mu::proto::xor3_apply(data.data(), data.size(),
                          mu::proto::keys::kXor3.data());
    assert_bytes_eq(data, expected, "xor3 'Devin01'");
}

TEST(xor3_is_symmetric) {
    std::vector<std::uint8_t> rng;
    for (int i = 0; i < 64; ++i) rng.push_back(static_cast<std::uint8_t>(i * 7));
    auto original = rng;
    mu::proto::xor3_apply(rng.data(), rng.size(),
                          mu::proto::keys::kXor3.data());
    mu::proto::xor3_apply(rng.data(), rng.size(),
                          mu::proto::keys::kXor3.data());
    assert_bytes_eq(rng, original, "xor3 round-trip");
}

// ---------------------------------------------------------------------
// Xor32
// ---------------------------------------------------------------------

TEST(xor32_round_trip) {
    // Round-trip a C1 packet through encrypt -> decrypt and verify we
    // get back the original.
    std::vector<std::uint8_t> packet = {
        0xC1, 0x10, 0xF1, 0x01, 0x02, 0x03, 0x04, 0x05,
        0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D};
    auto original = packet;

    mu::proto::xor32_encrypt(packet.data(), packet.size(),
                             mu::proto::keys::kXor32.data());
    // The prefix, length and type bytes must not be touched.
    assert_eq((int)packet[0], 0xC1, "prefix preserved");
    assert_eq((int)packet[1], 0x10, "length preserved");
    assert_eq((int)packet[2], 0xF1, "type preserved");

    mu::proto::xor32_decrypt(packet.data(), packet.size(),
                             mu::proto::keys::kXor32.data());
    assert_bytes_eq(packet, original, "xor32 round-trip");
}

// ---------------------------------------------------------------------
// SimpleModulus
// ---------------------------------------------------------------------

TEST(simple_modulus_c1_passthrough) {
    // C1 packets are NOT encrypted by SimpleModulus — they should
    // round-trip verbatim (mirrors OpenMU's C1NonEncryptAsync test).
    std::vector<std::uint8_t> packet = {0xC1, 0x05, 0xAB, 0xBC, 0xDE};
    mu::proto::SimpleModulusEncryptor enc;
    auto encoded = enc.encrypt(packet.data(), packet.size());
    assert_bytes_eq(encoded, packet, "C1 pass-through");
}

TEST(simple_modulus_c2_passthrough) {
    std::vector<std::uint8_t> packet = {0xC2, 0x00, 0x05, 0xBC, 0xDE};
    mu::proto::SimpleModulusEncryptor enc;
    auto encoded = enc.encrypt(packet.data(), packet.size());
    assert_bytes_eq(encoded, packet, "C2 pass-through");
}

TEST(simple_modulus_roundtrip_short) {
    // Encrypt + decrypt a small C3 packet.
    //
    // IMPORTANT: client-encrypt and client-decrypt key sets are NOT a
    // matched pair — they're the two halves of a duplex channel.  To
    // round-trip a client-encrypted packet locally we need the
    // *server* decrypt key (the one OpenMU's server uses for
    // incoming-from-client packets).  This is exactly the pairing
    // used by OpenMU's `PipelinedEncryptDecryptCycleTests`.
    std::vector<std::uint8_t> packet = {0xC3, 0x05, 0xAB, 0xBC, 0xDE};

    mu::proto::SimpleModulusEncryptor enc;
    mu::proto::SimpleModulusDecryptor dec(
        mu::proto::keys::kSimpleModulusServerDecrypt.data());

    auto encoded = enc.encrypt(packet.data(), packet.size());
    if (encoded.size() <= packet.size()) {
        fail("encrypted packet must be larger than plaintext for C3");
    }
    assert_eq((int)encoded[0], 0xC3, "C3 prefix preserved");

    auto decoded = dec.decrypt(encoded.data(), encoded.size());
    assert_bytes_eq(decoded, packet, "C3 round-trip");
}

TEST(simple_modulus_roundtrip_multiblock) {
    // 24-byte payload spans 4 cipher blocks: counter+7, then 8, 8, 1.
    std::vector<std::uint8_t> packet;
    packet.push_back(0xC3);
    packet.push_back(0x00);  // placeholder
    for (int i = 0; i < 22; ++i) {
        packet.push_back(static_cast<std::uint8_t>(0x10 + i));
    }
    mu::proto::write_packet_size(packet.data(), packet.size());

    mu::proto::SimpleModulusEncryptor enc;
    mu::proto::SimpleModulusDecryptor dec(
        mu::proto::keys::kSimpleModulusServerDecrypt.data());

    auto encoded = enc.encrypt(packet.data(), packet.size());
    auto decoded = dec.decrypt(encoded.data(), encoded.size());
    assert_bytes_eq(decoded, packet, "multi-block round-trip");
}

TEST(simple_modulus_counter_advances) {
    // The block counter must monotonically increase across multiple
    // encrypts within a single connection.  We verify that two
    // successive encrypts of the same plaintext produce DIFFERENT
    // ciphertexts (because the counter byte changed inside block 0).
    std::vector<std::uint8_t> packet = {0xC3, 0x06, 0x10, 0x20, 0x30, 0x40};
    mu::proto::SimpleModulusEncryptor enc;
    auto a = enc.encrypt(packet.data(), packet.size());
    auto b = enc.encrypt(packet.data(), packet.size());
    if (a == b) fail("encrypt with advancing counter must differ");
}

TEST(simple_modulus_decrypt_wrong_counter_throws) {
    // If the server (in our case the decryptor) sees an out-of-order
    // counter we must reject the packet.  Encrypt twice without
    // calling reset() on the decryptor — the decryptor will be
    // expecting counter=0 first, then 1, so feeding it the second
    // ciphertext first should throw.
    std::vector<std::uint8_t> packet = {0xC3, 0x06, 0x10, 0x20, 0x30, 0x40};
    mu::proto::SimpleModulusEncryptor enc;
    auto first  = enc.encrypt(packet.data(), packet.size());
    auto second = enc.encrypt(packet.data(), packet.size());

    mu::proto::SimpleModulusDecryptor dec(
        mu::proto::keys::kSimpleModulusServerDecrypt.data());
    bool threw = false;
    try {
        dec.decrypt(second.data(), second.size());
    } catch (const std::runtime_error&) {
        threw = true;
    }
    if (!threw) fail("decrypt must reject out-of-order counter");

    // Reset state and verify the in-order replay still works.
    dec.reset();
    auto p1 = dec.decrypt(first.data(), first.size());
    auto p2 = dec.decrypt(second.data(), second.size());
    assert_bytes_eq(p1, packet, "in-order decrypt #1");
    assert_bytes_eq(p2, packet, "in-order decrypt #2");
}

TEST(simple_modulus_decrypt_zero_first_block_size_rejected) {
    // Regression test for a buffer-overflow hardening fix.  A
    // malformed cipher whose first block decodes to block_size == 0
    // must be rejected at the decryptor; if it slipped through, the
    // subsequent `memcpy(..., bs - 1)` would wrap to SIZE_MAX (a
    // remotely triggerable heap overflow on attacker-controlled
    // network input).
    //
    // We forge a malformed packet by encrypting a real block, then
    // overwriting the block-size trailer (byte 9) so the decoded
    // block_size becomes 0 while the checksum still matches.  We
    // accept-wrong-checksum on the decryptor so the size check is
    // exercised in isolation.
    std::vector<std::uint8_t> packet = {0xC3, 0x06, 0x10, 0x20, 0x30, 0x40};
    mu::proto::SimpleModulusEncryptor enc;
    auto cipher = enc.encrypt(packet.data(), packet.size());

    // header is 2 for C3, first block starts at offset 2 and occupies
    // 11 bytes.  Bytes 9 and 10 of that block are the size trailer and
    // checksum.  Flip the size byte so the decoded size becomes 0
    // (size XOR sum XOR 0x3D == 0  =>  size = sum ^ 0x3D).
    const std::uint8_t checksum = cipher[2 + 10];
    cipher[2 + 9] = static_cast<std::uint8_t>(checksum ^ 0x3D);

    mu::proto::SimpleModulusDecryptor dec(
        mu::proto::keys::kSimpleModulusServerDecrypt.data());
    dec.set_accept_wrong_checksum(true);  // isolate the size check

    bool threw = false;
    try {
        dec.decrypt(cipher.data(), cipher.size());
    } catch (const std::runtime_error&) {
        threw = true;
    }
    if (!threw) fail("decrypt must reject first block with size 0");
}

// ---------------------------------------------------------------------
// Combined Xor32 + SimpleModulus round-trip (full client send path)
// ---------------------------------------------------------------------

TEST(full_pipeline_round_trip) {
    // Models the real OpenMU client outbound chain (see
    // Season6Episode3NetworkEncryptionFactoryPlugIn):
    //   cipher = SimpleModulus_enc( Xor32_enc( plaintext ) )
    // The matching server-side decrypt chain is:
    //   plain  = Xor32_dec( SimpleModulus_dec( cipher ) )
    // Encrypting then decrypting must round-trip to the original.
    std::vector<std::uint8_t> packet;
    packet.push_back(0xC3);
    packet.push_back(0x00);
    for (int i = 0; i < 30; ++i) {
        packet.push_back(static_cast<std::uint8_t>(0x40 + i));
    }
    mu::proto::write_packet_size(packet.data(), packet.size());

    mu::proto::SimpleModulusEncryptor enc;
    mu::proto::SimpleModulusDecryptor dec(
        mu::proto::keys::kSimpleModulusServerDecrypt.data());

    // -- outbound (client side) --
    auto stage1 = packet;
    mu::proto::xor32_encrypt(stage1.data(), stage1.size(),
                             mu::proto::keys::kXor32.data());
    auto cipher = enc.encrypt(stage1.data(), stage1.size());

    if (cipher == packet) fail("cipher must not equal plaintext");

    // -- inbound (server side) --
    auto intermediate = dec.decrypt(cipher.data(), cipher.size());
    mu::proto::xor32_decrypt(intermediate.data(), intermediate.size(),
                             mu::proto::keys::kXor32.data());

    assert_bytes_eq(intermediate, packet, "full pipeline round-trip");
}

// ---------------------------------------------------------------------
// Compatibility with OpenMU reference vector
// ---------------------------------------------------------------------

// These two strings are copied verbatim from MUnique/OpenMU
// `tests/MUnique.OpenMU.Network.Tests/PipelinedEncryptorTests.cs:48-49`
// (test `C3EncryptAsync`).  The C# server-side encryptor takes
// `packet` and emits `expected`.  The corresponding *client-side*
// encrypt key is the one we use by default — the C# test uses the
// server's DefaultServerKey, so we round-trip via *our own* keys
// instead of trying to bit-match a server-keyed ciphertext.  What we
// CAN verify byte-for-byte from this vector is the length / framing
// shape: the encrypted form must start with 0xC3 and the size field
// must match the SimpleModulus output for the same plaintext.
TEST(openmu_c3_size_matches_reference) {
    const std::string plain_b64 =
        "w7kxFgK8hYpGGLgdXe7ZpTZViB+r3sRI3YSqZs7/Mh5Vmh2mXqs+3dqkvURmXrL"
        "57ASs+FkJz/236Tl9ER67R+WZyMLRMkeLF6tEBiB/4X7SsXrKUznES8of73Rxw"
        "My76HZezJbvJ7m9IOGuxcjcNwe6q1+k8fOs1Hz3sULSGlbfiB6qIBXo4onADTNY"
        "FoYCQrdtthVsF/aDsvcZ93V36gaKzzyqMhby0sjV4+TAU7719W6LZWNAcnA=";
    const std::string cipher_b64 =
        "w/+eRi3xwRp1LdEdKA9lEFFECgL0vYm8siQHloDxwRXsKx4SdRNuBxgl3W3N+OcgJ"
        "y/dTaThrSAVUhV1XkPLtCklDzoX4gjPXGPjVEJIfb0iY+wGCIFSZnZDKvYIh8GIF"
        "7CNZlOLxigIGPESgPY2Ax6WBoTZaDexFePWS8Q1i0Phk5XkZ1LqTRG5gwwxvCZzR"
        "i04HVRMTleEnUN2IOIF79s2xr8BXWhsbTIUx30ychj0wdeeAz8D2DCFUUdyB6kWo"
        "MG/4V7Mu44JrgM3mfiD0py6j145biJC/BDr9Ii9AsEokQX15FptGi+9/C64i7EBH"
        "7QPOm69cdjeNFBQPpms";

    auto plain = b64decode(plain_b64);
    auto cipher = b64decode(cipher_b64);

    // Sanity-check the reference vector itself.
    assert_eq((int)plain[0], 0xC3, "OpenMU plain prefix");
    assert_eq((int)cipher[0], 0xC3, "OpenMU cipher prefix");
    assert_eq(mu::proto::read_packet_size(plain.data(), plain.size()),
              static_cast<int>(plain.size()),
              "OpenMU plaintext self-describes size");
    assert_eq(mu::proto::read_packet_size(cipher.data(), cipher.size()),
              static_cast<int>(cipher.size()),
              "OpenMU ciphertext self-describes size");

    // The exact ciphertext bytes depend on the server-side key the
    // upstream test uses, which is intentionally NOT the same as our
    // client default — but the OUTPUT SIZE is determined purely by the
    // input size and the block layout, so a matching shape proves our
    // framing math agrees with OpenMU's.
    mu::proto::SimpleModulusEncryptor enc;
    auto our_cipher = enc.encrypt(plain.data(), plain.size());
    assert_eq(our_cipher.size(), cipher.size(),
              "ciphertext size matches OpenMU reference");

    // And the same plaintext + matching server-side decrypt key must
    // round-trip back to the original.
    mu::proto::SimpleModulusDecryptor dec(
        mu::proto::keys::kSimpleModulusServerDecrypt.data());
    auto round = dec.decrypt(our_cipher.data(), our_cipher.size());
    assert_bytes_eq(round, plain, "OpenMU plaintext round-trip");
}

// Byte-exact compatibility test: feed an OpenMU-encrypted C3 packet
// through the full server-side decrypt pipeline (SimpleModulus then
// Xor32) and confirm we recover the exact plaintext.  Source:
// MUnique/OpenMU `tests/.../PipelinedDecryptorTests.cs`
// (`C3DecryptAsync`).  OpenMU's `PipelinedDecryptor` chains a
// `PipelinedSimpleModulusDecryptor(DefaultServerKey)` and a
// `PipelinedXor32Decryptor(DefaultKeys.Xor32Key)` in that order on
// inbound data.  A successful decode proves byte-for-byte interop
// with the upstream reference implementation.
TEST(openmu_c3_decrypt_byte_exact) {
    const std::string plain_b64 =
        "w7gAudHEjjSP53H6Rkp3oXj7B9z+rVDR2f0Is4bvsIsUL3RM/aTDB2FX9YG3Hkbo"
        "y1Z1JThot558MeDTvNuunzfl5RbWK6TTOP97prjPGbq3IOcweopTq3fVz8vD8Eu"
        "FqVVJ0jgvEZ+xoe047RHmrRgmG5zzfSWtkTmeAVzZD0i09f1jhUeBiA5HfticGr"
        "5m7iGzndSvkSwvm0D/kRBD15GlhPgTgyfQpJONrP5NEHd7NxI6JnJzBQ==";
    const std::string cipher_b64 =
        "w/+r1UUAxjcz8wQKPyQ8IsrH0PAGfEF0+VwSJx3GwXzcOw42Oj7OVZIU/EXp3FMY"
        "ZIbCGlPGNTwJuaEKkEHWoaiNtoM8NwWwhcPBmITN+C2nH48ZhNDtNQYzOOd3stx"
        "bIPwkuI1LmgohXa1iHnUUIdLeQ3+h1oURffLHjnkZcpyuECtoLRjgYjaayychmM"
        "DG8wVPVPKokNIrLIeyyFJGLMv0pgH8FyIX5mCJqOog/B26j44cU0eKyQK6geDV7"
        "f5xMxtlYL1gy/4ZcyLexoEAdeGSp5QmHLhCb8L99cD1UVIul9IQkU35AzYhRRtU"
        "7yjTjYlhVLjCMiIVnjQfJKac";

    auto plain = b64decode(plain_b64);
    auto cipher = b64decode(cipher_b64);

    // Stage 1: SimpleModulus decrypt with server key.
    mu::proto::SimpleModulusDecryptor dec(
        mu::proto::keys::kSimpleModulusServerDecrypt.data());
    auto intermediate = dec.decrypt(cipher.data(), cipher.size());

    // Stage 2: Xor32 decrypt with the default 32-byte key.
    mu::proto::xor32_decrypt(intermediate.data(), intermediate.size(),
                             mu::proto::keys::kXor32.data());

    assert_bytes_eq(intermediate, plain,
                    "OpenMU PipelinedDecryptorTests.C3DecryptAsync vector");
}

}  // namespace

int main() {
    int passed = 0;
    int failed = 0;
    for (const auto& t : tests()) {
        try {
            t.body();
            std::printf("  [ok]   %s\n", t.name);
            ++passed;
        } catch (const std::exception& e) {
            std::printf("  [FAIL] %s: %s\n", t.name, e.what());
            ++failed;
        }
    }
    std::printf("\nprotocol_tests: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
