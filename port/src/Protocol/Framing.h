#pragma once

// MU Online packet framing: C1 / C2 / C3 / C4 prefix bytes.
//
// Reference: MUnique/OpenMU `src/Network/ArrayExtensions.cs` —
// `GetPacketHeaderSize`, `GetPacketSize`, `SetPacketSize`.
//
//   * 0xC1 — plaintext, 1-byte length, header = 2 bytes.
//   * 0xC3 — SimpleModulus-encrypted variant of C1, header = 2 bytes.
//   * 0xC2 — plaintext, 2-byte big-endian length, header = 3 bytes.
//   * 0xC4 — SimpleModulus-encrypted variant of C2, header = 3 bytes.
//
// The reported length is always the total packet size including the
// header. The packet type lives at offset [header_size].
//
// We deliberately mirror OpenMU's API surface (byte-prefix dispatch,
// header sizes, in-place size patching) so any extension test vector
// can be ported verbatim.

#include <cstddef>
#include <cstdint>

namespace mu::proto {

inline constexpr std::uint8_t kPrefixC1 = 0xC1;
inline constexpr std::uint8_t kPrefixC2 = 0xC2;
inline constexpr std::uint8_t kPrefixC3 = 0xC3;
inline constexpr std::uint8_t kPrefixC4 = 0xC4;

// Size in bytes of the framing header for a given prefix byte.
// Returns 0 for unknown prefixes — callers should treat that as an
// invalid header (OpenMU throws InvalidPacketHeaderException in the
// same situation).
int header_size_for_prefix(std::uint8_t prefix) noexcept;

// Reads the total packet size encoded in `data`'s framing header.
// Requires `len >= 3` (the worst case header size). Returns 0 for
// invalid prefixes.
int read_packet_size(const std::uint8_t* data, std::size_t len) noexcept;

// Writes the size of `data` back into its own framing header.
// Caller guarantees that `data[0]` is one of the four valid prefixes
// and that `len` matches the desired total packet size (header
// included). No-ops on unknown prefixes.
void write_packet_size(std::uint8_t* data, std::size_t len) noexcept;

// Whether this prefix marks a SimpleModulus-encrypted payload.
inline constexpr bool is_encrypted_prefix(std::uint8_t p) noexcept {
    return p == kPrefixC3 || p == kPrefixC4;
}

}  // namespace mu::proto
