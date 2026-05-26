#pragma once

// State machine for talking to an OpenMU ConnectServer (port 44405 on
// the reference server).  The ConnectServer is the first thing a MU
// client speaks to; its job is to:
//
//   1. Greet the client (server -> client `C1 04 00 01`).
//   2. Answer "list me the game servers" queries (`F4 06`).
//   3. Answer "what is server N's IP:port" queries (`F4 03`).
//
// Once the client has the IP:port of the desired game server, it
// disconnects from the ConnectServer and dials the game server with
// `Codec::GameServer` (Xor32 + SimpleModulus).
//
// This class is intentionally I/O-free: feed it inbound packets via
// `on_packet()`, drain outbound packets via `take_outbound()`.  The
// caller (`App`) shuttles bytes between this session and a
// `mu::proto::Connection` in `Codec::Plain` mode.

#include <cstdint>
#include <string>
#include <vector>

namespace mu::proto {

struct ConnectServerEntry {
    std::uint16_t id = 0;
    std::uint8_t  load_percent = 0;
};

class ConnectServerSession {
public:
    enum class Phase {
        WaitingForHello,
        RequestedServerList,
        ServerListReceived,
        RequestedConnectionInfo,
        ConnectionInfoReceived,
        Error,
    };

    // Process one fully-framed inbound packet.  Updates the phase and
    // may produce outbound packets that the caller must drain with
    // `take_outbound()`.
    //
    // Unknown packet types are logged into `last_unknown_` and ignored
    // -- the ConnectServer occasionally emits keep-alives or version
    // banners that we don't care about, and we don't want to bring the
    // whole session down for them.
    void on_packet(const std::uint8_t* data, std::size_t len);

    // Ask the connect server for the IP:port of server `id`.  The
    // server list response (phase ServerListReceived) is a prereq
    // -- but we don't enforce that, the caller can ask early if they
    // know the id out-of-band.
    void request_connection_info(std::uint16_t id);

    // Drain the queue of outbound packets the session wants the caller
    // to ship over `Connection::send_packet`.
    std::vector<std::vector<std::uint8_t>> take_outbound();

    Phase phase() const noexcept { return phase_; }

    const std::vector<ConnectServerEntry>& server_list() const noexcept {
        return server_list_;
    }

    // After phase == ConnectionInfoReceived these are populated.
    const std::string& game_server_host() const noexcept {
        return game_server_host_;
    }
    std::uint16_t game_server_port() const noexcept {
        return game_server_port_;
    }

    const std::string& last_error() const noexcept { return error_; }
    const std::string& last_unknown() const noexcept { return last_unknown_; }

private:
    void parse_server_list(const std::uint8_t* data, std::size_t len);
    void parse_connection_info(const std::uint8_t* data, std::size_t len);
    void fail(std::string msg);

    Phase phase_ = Phase::WaitingForHello;
    std::vector<std::vector<std::uint8_t>> outbound_;
    std::vector<ConnectServerEntry> server_list_;
    std::string game_server_host_;
    std::uint16_t game_server_port_ = 0;
    std::string error_;
    std::string last_unknown_;
};

}  // namespace mu::proto
