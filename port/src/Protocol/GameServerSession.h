#pragma once

// State machine for talking to an OpenMU GameServer (port 55901 on
// the reference server).  The client dials the GameServer after the
// ConnectServer has handed out its endpoint (see
// `ConnectServerSession`).
//
// The very first packet the server sends after TCP accept is a plain
// `C1` `F1 00` "GameServerEntered" frame containing a result byte, the
// session player id, and a 5-byte ASCII server-version string:
//
//   C1 0C F1 00  <result:1>  <playerId:2 BE>  <version:5 ASCII>
//
// This packet is **not** encrypted (it is a `C1` prefix, which both
// SimpleModulus and Xor32 pass through), so we can read it without
// any keys.  Subsequent protocol traffic (login, world enter, etc.) is
// `C3` and requires the M2 codec stack via
// `Connection(Codec::GameServer)` -- but that's M3+ territory; M2.6
// just stops after parsing the Entered packet so we have a verified
// foothold inside the encrypted protocol.

#include <cstdint>
#include <string>
#include <vector>

namespace mu::proto {

class GameServerSession {
public:
    enum class Phase {
        WaitingForEntered,
        Entered,
        Error,
    };

    // Process one fully-framed inbound packet.
    void on_packet(const std::uint8_t* data, std::size_t len);

    // Outbound queue.  Empty in M2.6; reserved for the login handshake
    // that will live in a future milestone.
    std::vector<std::vector<std::uint8_t>> take_outbound();

    Phase phase() const noexcept { return phase_; }

    // Populated after Phase::Entered.
    std::uint8_t  result() const noexcept       { return result_; }
    std::uint16_t player_id() const noexcept    { return player_id_; }
    const std::string& version() const noexcept { return version_; }

    const std::string& last_error() const noexcept { return error_; }
    const std::string& last_unknown() const noexcept { return last_unknown_; }

private:
    void parse_entered(const std::uint8_t* body, std::size_t len);
    void fail(std::string msg);

    Phase phase_ = Phase::WaitingForEntered;
    std::vector<std::vector<std::uint8_t>> outbound_;
    std::uint8_t  result_ = 0;
    std::uint16_t player_id_ = 0;
    std::string   version_;
    std::string   error_;
    std::string   last_unknown_;
};

}  // namespace mu::proto
