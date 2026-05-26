#include "Protocol/Connection.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

#include "Protocol/Framing.h"
#include "Protocol/Xor.h"

namespace mu::proto {

Connection::Connection(mu::net::TcpClient& tcp) : tcp_(tcp) {}

bool Connection::send_packet(const std::uint8_t* data, std::size_t len) {
    if (len < 2) return false;
    int header = header_size_for_prefix(data[0]);
    if (header == 0) return false;
    int declared = read_packet_size(data, len);
    if (declared <= 0 || static_cast<std::size_t>(declared) != len) {
        return false;
    }

    // Per OpenMU's Season6Episode3NetworkEncryptionFactoryPlugIn,
    // the client-side outbound pipeline is
    //   cipher = SimpleModulus_enc( Xor32_enc( plaintext ) )
    // The Xor32 layer is applied first to the framed plaintext (the
    // SimpleModulus block cipher then re-frames C3/C4 packets; C1/C2
    // pass through unchanged).
    std::vector<std::uint8_t> buf(data, data + len);
    xor32_encrypt(buf.data(), buf.size(), keys::kXor32.data());

    if (is_encrypted_prefix(buf[0])) {
        buf = enc_.encrypt(buf.data(), buf.size());
    }

    return tcp_.send(buf.data(), buf.size());
}

bool Connection::try_extract_one_packet(std::vector<std::uint8_t>& out) {
    if (inbound_.size() < 2) return false;
    int header = header_size_for_prefix(inbound_[0]);
    if (header == 0) {
        throw std::runtime_error("Connection: invalid framing prefix");
    }
    if (static_cast<int>(inbound_.size()) < header) return false;
    int total = read_packet_size(inbound_.data(), inbound_.size());
    if (total <= 0 || total < header) {
        throw std::runtime_error("Connection: invalid framing length");
    }
    if (static_cast<int>(inbound_.size()) < total) return false;

    out.assign(inbound_.begin(), inbound_.begin() + total);
    inbound_.erase(inbound_.begin(), inbound_.begin() + total);
    return true;
}

std::vector<std::vector<std::uint8_t>> Connection::poll_packets() {
    std::vector<std::vector<std::uint8_t>> result;
    std::lock_guard<std::mutex> lock(mu_);

    // Drain everything the TCP layer has received into our byte buffer.
    auto chunk = tcp_.drain_received();
    if (!chunk.empty()) {
        inbound_.insert(inbound_.end(), chunk.begin(), chunk.end());
    }

    // NOTE: inbound is intentionally NOT symmetric with `send_packet`.
    // The OpenMU Season 6 Ep 3 server-to-client direction uses ONLY
    // SimpleModulus (see `Season6Episode3NetworkEncryptionFactoryPlugIn
    // .CreateEncryptor(direction=ServerToClient)` -> `PipelinedEncryptor`
    // which is `PipelinedSimpleModulusEncryptor` with no Xor32 wrapper).
    // Applying `xor32_decrypt` here would break interop with a real
    // OpenMU server.
    std::vector<std::uint8_t> one;
    while (try_extract_one_packet(one)) {
        if (is_encrypted_prefix(one[0])) {
            one = dec_.decrypt(one.data(), one.size());
        }
        result.push_back(std::move(one));
        one.clear();
    }
    return result;
}

void Connection::reset_counters() noexcept {
    enc_.reset();
    dec_.reset();
}

}  // namespace mu::proto
