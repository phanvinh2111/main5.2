#pragma once

// Default crypto key material for the MU Online network protocol.
//
// Values are byte-identical to MUnique/OpenMU's reference C# code so
// that the client and the OpenMU server can interoperate without any
// configuration:
//
//   * `kXor3` — 3-byte XOR key from `src/Network/Xor/DefaultKeys.cs`.
//   * `kXor32` — 32-byte XOR key from the same file.
//   * `kSimpleModulusClientEncrypt` / `kSimpleModulusClientDecrypt`
//     — the client-side keys from
//     `PipelinedSimpleModulusEncryptor.DefaultClientKey` /
//     `PipelinedSimpleModulusDecryptor.DefaultClientKey` (Season 6.15
//     compatible default key set).
//
// These are public protocol constants, not secrets.

#include <array>
#include <cstdint>

namespace mu::proto::keys {

inline constexpr std::array<std::uint8_t, 3> kXor3 = {0xFC, 0xCF, 0xAB};

inline constexpr std::array<std::uint8_t, 32> kXor32 = {
    0xAB, 0x11, 0xCD, 0xFE, 0x18, 0x23, 0xC5, 0xA3,
    0xCA, 0x33, 0xC1, 0xCC, 0x66, 0x67, 0x21, 0xF3,
    0x32, 0x12, 0x15, 0x35, 0x29, 0xFF, 0xFE, 0x1D,
    0x44, 0xEF, 0xCD, 0x41, 0x26, 0x3C, 0x4E, 0x4D,
};

// SimpleModulus client encryption key (12 uint32 in the layout
// `{ mod0..mod3, enc0..enc3, xor0..xor3 }`).
//
// From OpenMU `PipelinedSimpleModulusEncryptor.DefaultClientKey`:
//   `SimpleModulusKeys.CreateEncryptionKeys(new uint[] {
//       128079, 164742, 70235, 106898,
//        23489,  11911, 19816,  13647,
//        48413,  46165, 15171,  37433 })`
inline constexpr std::array<std::uint32_t, 12> kSimpleModulusClientEncrypt = {
    128079, 164742,  70235, 106898,
     23489,  11911,  19816,  13647,
     48413,  46165,  15171,  37433,
};

// SimpleModulus client decryption key (12 uint32 in the layout
// `{ mod0..mod3, dec0..dec3, xor0..xor3 }`).
//
// From OpenMU `PipelinedSimpleModulusDecryptor.DefaultClientKey`:
//   `SimpleModulusKeys.CreateDecryptionKeys(new uint[] {
//        73326, 109989, 98843, 171058,
//        18035,  30340, 24701,  11141,
//        62004,  64409, 35374,  64599 })`
inline constexpr std::array<std::uint32_t, 12> kSimpleModulusClientDecrypt = {
    73326, 109989,  98843, 171058,
    18035,  30340,  24701,  11141,
    62004,  64409,  35374,  64599,
};

// SimpleModulus server-side decrypt key — the inverse of
// `kSimpleModulusClientEncrypt`.  Same modulus and xor as the client
// encryptor; only the multiplicative inverses differ.  From OpenMU
// `PipelinedSimpleModulusDecryptor.DefaultServerKey`:
//   `SimpleModulusKeys.CreateDecryptionKeys(new uint[] {
//        128079, 164742, 70235, 106898,
//         31544,   2047, 57011,  10183,
//         48413,  46165, 15171,  37433 })`
//
// In production this lives on the server, not the client; the port
// keeps a copy here so the codec unit tests can prove round-trip
// correctness without spinning up a second machine.
inline constexpr std::array<std::uint32_t, 12> kSimpleModulusServerDecrypt = {
    128079, 164742,  70235, 106898,
     31544,   2047,  57011,  10183,
     48413,  46165,  15171,  37433,
};

// SimpleModulus server-side encrypt key — inverse of
// `kSimpleModulusClientDecrypt`.  Mirrors OpenMU
// `PipelinedSimpleModulusEncryptor.DefaultServerKey`:
//   `SimpleModulusKeys.CreateEncryptionKeys(new uint[] {
//        73326, 109989, 98843, 171058,
//        13169,  19036, 35482,  29587,
//        62004,  64409, 35374,  64599 })`
inline constexpr std::array<std::uint32_t, 12> kSimpleModulusServerEncrypt = {
    73326, 109989,  98843, 171058,
    13169,  19036,  35482,  29587,
    62004,  64409,  35374,  64599,
};

}  // namespace mu::proto::keys
