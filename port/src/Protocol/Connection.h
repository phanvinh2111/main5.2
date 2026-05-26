#pragma once

// MU Online network "connection" — composes the codec stack on top of
// a portable TcpClient.
//
// Outgoing (client → server) per OpenMU server convention:
//
//   raw bytes → SimpleModulus.encrypt (C3/C4 only) → Xor32 → TCP send
//
// Incoming (server → client):
//
//   TCP recv → reassemble into framed packets → SimpleModulus.decrypt
//   (C3/C4 only) → deliver
//
// Xor3 obfuscation of credential / sensitive sub-fields is applied
// per-packet by the caller before queuing the packet, because the
// start-offset and length depend on the packet type.  The codec layer
// only handles per-packet transforms.

#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

#include "Network/TcpClient.h"
#include "Protocol/SimpleModulus.h"

namespace mu::proto {

class Connection {
public:
    explicit Connection(mu::net::TcpClient& tcp);

    // Encode + queue a complete framed packet for transmission.
    // Returns false if the packet looks malformed (unknown prefix,
    // length field doesn't match the buffer length, etc.).
    bool send_packet(const std::uint8_t* data, std::size_t len);

    // Poll the underlying TCP client and return any complete packets
    // that have been received and decoded.  Each returned vector is a
    // single framed packet with a valid C1/C2/C3 (or C4) header and an
    // up-to-date length field — but with the SimpleModulus layer
    // already removed (so a C3 ciphertext becomes a C3-prefixed
    // plaintext, NOT a C1).
    //
    // Returns an empty list if nothing new is available.  Throws
    // std::runtime_error on malformed framing.
    std::vector<std::vector<std::uint8_t>> poll_packets();

    // For tests: drop the SimpleModulus block counter back to 0.
    void reset_counters() noexcept;

private:
    bool try_extract_one_packet(std::vector<std::uint8_t>& out);

    mu::net::TcpClient& tcp_;
    SimpleModulusEncryptor enc_;
    SimpleModulusDecryptor dec_;

    // Inbound byte stream buffer; we accumulate raw recv() bytes here
    // and try to peel off framed packets one at a time.
    std::vector<std::uint8_t> inbound_;
    std::mutex mu_;
};

}  // namespace mu::proto
