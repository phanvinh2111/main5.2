#include "Protocol/GameServerSession.h"

#include "Protocol/Framing.h"

#include <cstdio>

namespace mu::proto {

namespace {

constexpr std::uint8_t kHeadEntered = 0xF1;
constexpr std::uint8_t kSubEntered  = 0x00;

std::uint16_t read_u16_be(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(p[0]) << 8) | p[1]);
}

int headcode_offset(std::uint8_t prefix) noexcept {
    const int header = header_size_for_prefix(prefix);
    return header > 0 ? header : 0;
}

}  // namespace

void GameServerSession::on_packet(const std::uint8_t* data, std::size_t len) {
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

    if (head == kHeadEntered && sub == kSubEntered) {
        parse_entered(data + hc_off + 2, len - (hc_off + 2));
        return;
    }

    // Anything else is interesting but not necessarily fatal: keep
    // around for diagnostics.  M3+ will route these to dedicated
    // handlers (login response, character list, ...).
    char buf[64];
    std::snprintf(buf, sizeof(buf), "head=0x%02X sub=0x%02X len=%zu",
                  head, sub, len);
    last_unknown_ = buf;
}

std::vector<std::vector<std::uint8_t>> GameServerSession::take_outbound() {
    auto out = std::move(outbound_);
    outbound_.clear();
    return out;
}

void GameServerSession::parse_entered(const std::uint8_t* body,
                                      std::size_t len) {
    // result (1) + playerId (2 BE) + version (5 ASCII) = 8 bytes
    if (len < 8) {
        fail("GameServerEntered: payload too short");
        return;
    }
    result_    = body[0];
    player_id_ = read_u16_be(body + 1);
    version_.assign(reinterpret_cast<const char*>(body + 3), 5);
    phase_ = Phase::Entered;
}

void GameServerSession::fail(std::string msg) {
    error_ = std::move(msg);
    phase_ = Phase::Error;
}

}  // namespace mu::proto
