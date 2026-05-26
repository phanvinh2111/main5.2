#include "Protocol/Xor.h"

#include "Protocol/Framing.h"

namespace mu::proto {

void xor3_apply(std::uint8_t* data, std::size_t len,
                const std::uint8_t key[3]) noexcept {
    for (std::size_t i = 0; i < len; ++i) {
        data[i] = static_cast<std::uint8_t>(data[i] ^ key[i % 3]);
    }
}

void xor32_encrypt(std::uint8_t* packet, std::size_t len,
                   const std::uint8_t key[32]) noexcept {
    if (len < 2) return;
    int header_size = header_size_for_prefix(packet[0]);
    if (header_size == 0) return;

    // start at (header_size + 1) — the byte after the packet type, just
    // like OpenMU's PipelinedXor32Encryptor.
    for (std::size_t i = static_cast<std::size_t>(header_size + 1);
         i < len; ++i) {
        packet[i] = static_cast<std::uint8_t>(
            packet[i] ^ packet[i - 1] ^ key[i % 32]);
    }
}

void xor32_decrypt(std::uint8_t* packet, std::size_t len,
                   const std::uint8_t key[32]) noexcept {
    if (len < 2) return;
    int header_size = header_size_for_prefix(packet[0]);
    if (header_size == 0) return;

    // walk back-to-front so packet[i-1] is still the cipher byte when
    // we XOR it in (matches OpenMU PipelinedXor32Decryptor).
    if (len <= static_cast<std::size_t>(header_size) + 1) return;
    for (std::size_t i = len - 1;
         i > static_cast<std::size_t>(header_size); --i) {
        packet[i] = static_cast<std::uint8_t>(
            packet[i] ^ packet[i - 1] ^ key[i % 32]);
    }
}

}  // namespace mu::proto
