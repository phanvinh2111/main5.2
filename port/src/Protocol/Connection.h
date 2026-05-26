#pragma once

// MU Online network "connection" — composes the codec stack on top of
// a portable TcpClient.
//
// Two codec modes are supported:
//
//   * Codec::GameServer  — the Season 6 Episode 3 game-server protocol.
//     Outbound: Xor32 → SimpleModulus.encrypt (C3/C4 only) → TCP send.
//     Inbound:  TCP recv → framing → SimpleModulus.decrypt (C3/C4 only).
//     Matches `MUnique.OpenMU.Network.PlugIns
//     .Season6Episode3NetworkEncryptionFactoryPlugIn`.
//
//   * Codec::Plain       — the ConnectServer protocol (port 44405 on the
//     reference server).  No encryption layer; packets travel verbatim.
//     OpenMU's ConnectServer ships with the `PlainNetworkEncryption`
//     factory which is exactly this.
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
    enum class Codec {
        Plain,       // ConnectServer protocol — no crypto.
        GameServer,  // Season 6 Episode 3 game protocol — Xor32 + SimpleModulus.
    };

    explicit Connection(mu::net::TcpClient& tcp,
                        Codec codec = Codec::GameServer);

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
    Codec codec_;
    SimpleModulusEncryptor enc_;
    SimpleModulusDecryptor dec_;

    // Inbound byte stream buffer; we accumulate raw recv() bytes here
    // and try to peel off framed packets one at a time.
    std::vector<std::uint8_t> inbound_;
    std::mutex mu_;
};

}  // namespace mu::proto
