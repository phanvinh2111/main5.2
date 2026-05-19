#pragma once

// XOR-based field obfuscation, mirroring OpenMU's
// `src/Network/Xor/Xor3Encryptor.cs` and
// `src/Network/Xor/PipelinedXor32Encryptor.cs` byte-for-byte.

#include <cstddef>
#include <cstdint>

#include "Protocol/Keys.h"

namespace mu::proto {

// Symmetric 3-byte XOR applied in place to `[data, data+len)`.
// Used for short fields such as account name / password, the
// "username box" obfuscation pattern that the client and the OpenMU
// server both implement.  Pass the OpenMU default `kXor3` for the
// standard MU protocol.
void xor3_apply(std::uint8_t* data, std::size_t len,
                const std::uint8_t key[3]) noexcept;

// Apply the 32-byte rolling-XOR client-side encryption to a complete
// framed packet in place.  Equivalent to OpenMU's
// `PipelinedXor32Encryptor.EncryptAndWrite()`:
//
//   for i in (header_size + 1) .. (packet_size - 1):
//       packet[i] = packet[i] ^ packet[i-1] ^ key[i % 32]
//
// `packet[0]` is the C1/C2/C3/C4 prefix; size bytes and the packet
// type are *not* touched (the server needs the type in cleartext to
// dispatch).  No-op for unknown prefixes.
void xor32_encrypt(std::uint8_t* packet, std::size_t len,
                   const std::uint8_t key[32]) noexcept;

// Reverse the Xor32 transform.  Mirrors OpenMU's
// `PipelinedXor32Decryptor.DecryptAndWrite()` which walks the bytes
// back-to-front.
void xor32_decrypt(std::uint8_t* packet, std::size_t len,
                   const std::uint8_t key[32]) noexcept;

}  // namespace mu::proto
