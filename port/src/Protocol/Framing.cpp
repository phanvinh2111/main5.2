#include "Protocol/Framing.h"

namespace mu::proto {

int header_size_for_prefix(std::uint8_t prefix) noexcept {
    switch (prefix) {
        case kPrefixC1:
        case kPrefixC3:
            return 2;
        case kPrefixC2:
        case kPrefixC4:
            return 3;
        default:
            return 0;
    }
}

int read_packet_size(const std::uint8_t* data, std::size_t len) noexcept {
    if (len < 2) return 0;
    switch (data[0]) {
        case kPrefixC1:
        case kPrefixC3:
            return data[1];
        case kPrefixC2:
        case kPrefixC4:
            if (len < 3) return 0;
            return (static_cast<int>(data[1]) << 8) | data[2];
        default:
            return 0;
    }
}

void write_packet_size(std::uint8_t* data, std::size_t len) noexcept {
    if (len < 2) return;
    switch (data[0]) {
        case kPrefixC1:
        case kPrefixC3:
            data[1] = static_cast<std::uint8_t>(len & 0xFF);
            break;
        case kPrefixC2:
        case kPrefixC4:
            if (len < 3) return;
            data[1] = static_cast<std::uint8_t>((len >> 8) & 0xFF);
            data[2] = static_cast<std::uint8_t>(len & 0xFF);
            break;
        default:
            break;
    }
}

}  // namespace mu::proto
