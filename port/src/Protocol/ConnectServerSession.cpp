#include "Protocol/ConnectServerSession.h"

#include "Protocol/Framing.h"

#include <cstdio>
#include <cstring>

namespace mu::proto {

namespace {

constexpr std::uint8_t kHeadHello       = 0x00;
constexpr std::uint8_t kSubHello        = 0x01;

constexpr std::uint8_t kHeadConnectServer = 0xF4;
constexpr std::uint8_t kSubServerList     = 0x06;  // request + response
constexpr std::uint8_t kSubConnectionInfo = 0x03;  // request + response

// Big-endian 16-bit read.  OpenMU's connect-server packets use BE
// shorts everywhere (`ServerListResponse.ServerCount` is ShortBigEndian
// in the .pd schema).
std::uint16_t read_u16_be(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(p[0]) << 8) | p[1]);
}

// Returns offset of the `headcode` byte in a framed packet (1 for C1,
// 2 for C2; 0 for unknown).  Wraps `header_size_for_prefix` minus one
// for the headcode position itself.
int headcode_offset(std::uint8_t prefix) noexcept {
    const int header = header_size_for_prefix(prefix);
    return header > 0 ? header : 0;
}

}  // namespace

void ConnectServerSession::on_packet(const std::uint8_t* data, std::size_t len) {
    if (len < 4) {
        fail("packet too short");
        return;
    }
    const int hc_off = headcode_offset(data[0]);
    if (hc_off == 0 || len < static_cast<std::size_t>(hc_off + 2)) {
        fail("packet has unknown framing prefix");
        return;
    }
    const std::uint8_t head = data[hc_off];
    const std::uint8_t sub  = data[hc_off + 1];

    if (head == kHeadHello && sub == kSubHello) {
        // Server greeting.  Only react during the initial wait; later
        // hello echoes are ignored.  The reference server at
        // 180.93.43.39:44405 re-emits `C1 04 00 01` immediately after a
        // ConnectionInfo response -- we don't want to bounce back into
        // a ServerList loop just because of that.
        if (phase_ == Phase::WaitingForHello) {
            std::vector<std::uint8_t> req = {0xC1, 0x04, kHeadConnectServer,
                                             kSubServerList};
            outbound_.push_back(std::move(req));
            phase_ = Phase::RequestedServerList;
        }
        return;
    }

    if (head == kHeadConnectServer && sub == kSubServerList) {
        parse_server_list(data + hc_off + 2, len - (hc_off + 2));
        return;
    }

    if (head == kHeadConnectServer && sub == kSubConnectionInfo) {
        parse_connection_info(data + hc_off + 2, len - (hc_off + 2));
        return;
    }

    // Unknown but not fatal -- stash for diagnostics.
    char buf[64];
    std::snprintf(buf, sizeof(buf), "head=0x%02X sub=0x%02X len=%zu",
                  head, sub, len);
    last_unknown_ = buf;
}

void ConnectServerSession::request_connection_info(std::uint16_t id) {
    // C1 06 F4 03 <id_hi> <id_lo>
    std::vector<std::uint8_t> req = {
        0xC1, 0x06, kHeadConnectServer, kSubConnectionInfo,
        static_cast<std::uint8_t>((id >> 8) & 0xFF),
        static_cast<std::uint8_t>(id & 0xFF)
    };
    outbound_.push_back(std::move(req));
    phase_ = Phase::RequestedConnectionInfo;
}

std::vector<std::vector<std::uint8_t>> ConnectServerSession::take_outbound() {
    auto out = std::move(outbound_);
    outbound_.clear();
    return out;
}

void ConnectServerSession::parse_server_list(const std::uint8_t* body,
                                             std::size_t len) {
    if (len < 2) {
        fail("server list response missing count");
        return;
    }
    const std::uint16_t count = read_u16_be(body);
    const std::size_t entries_bytes = static_cast<std::size_t>(count) * 4;
    if (len < 2 + entries_bytes) {
        fail("server list response truncated");
        return;
    }
    server_list_.clear();
    server_list_.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i) {
        const std::uint8_t* p = body + 2 + i * 4;
        ConnectServerEntry e;
        e.id = read_u16_be(p);
        e.load_percent = p[2];
        // p[3] is a reserved byte in OpenMU's schema; ignored.
        server_list_.push_back(e);
    }
    phase_ = Phase::ServerListReceived;
}

void ConnectServerSession::parse_connection_info(const std::uint8_t* body,
                                                 std::size_t len) {
    // OpenMU `ConnectionInfo` payload is a 16-byte IP / hostname string
    // (null padded) followed by a 2-byte little-endian port.
    if (len < 18) {
        fail("connection info response truncated");
        return;
    }
    const std::size_t kIpLen = 16;
    // The IP is null-terminated within its 16-byte slot.
    std::size_t ip_str_len = 0;
    while (ip_str_len < kIpLen && body[ip_str_len] != 0) {
        ++ip_str_len;
    }
    game_server_host_.assign(reinterpret_cast<const char*>(body), ip_str_len);

    const std::uint8_t* port_p = body + kIpLen;
    game_server_port_ = static_cast<std::uint16_t>(
        port_p[0] | (static_cast<std::uint16_t>(port_p[1]) << 8));
    phase_ = Phase::ConnectionInfoReceived;
}

void ConnectServerSession::fail(std::string msg) {
    error_ = std::move(msg);
    phase_ = Phase::Error;
}

}  // namespace mu::proto
