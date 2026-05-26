#include "Protocol/GameServerSession.h"

#include "Protocol/Framing.h"
#include "Protocol/Keys.h"
#include "Protocol/Xor.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace mu::proto {

namespace {

constexpr std::uint8_t kHeadGameServerEntered = 0xF1;
constexpr std::uint8_t kSubGameServerEntered  = 0x00;
constexpr std::uint8_t kHeadLogin             = 0xF1;
constexpr std::uint8_t kSubLogin              = 0x01;
constexpr std::uint8_t kHeadCharacterList     = 0xF3;
constexpr std::uint8_t kSubCharacterList      = 0x00;

// RequestCharacterList wire layout (OpenMU
// `ClientToServer/ClientToServerPackets.xml`):
//
//   index | length | field
//   ------+--------+--------------------------------
//       0 |   1    | 0xC1 framing prefix
//       1 |   1    | length byte (0x05)
//       2 |   1    | head 0xF3
//       3 |   1    | sub  0x00
//       4 |   1    | language (echoed by server)
//
// Total = 5 bytes, plaintext (C1).
constexpr std::size_t kCharListReqLen      = 5;
constexpr std::size_t kCharListReqLangOff  = 4;

// CharacterList wire layout after the C1 header is stripped (the body
// passed to `parse_character_list`):
//
//   offset | length    | field
//   -------+-----------+----------------------------
//        0 |     1     | UnlockFlags
//        1 |     1     | MoveCnt
//        2 |     1     | CharacterCount (N)
//        3 |     1     | IsVaultExtended (0/1)
//        4 | N * S     | CharacterData entries
//
// S is either 34 (Classic / pre-Season-6) or 44 (Extended / Season 6+).
// Inside each CharacterData entry (matches OpenMU `CharacterData`):
//
//   inner | length | field
//   ------+--------+----------------------------
//      0  |   1    | SlotIndex
//      1  |  10    | Name (ASCII, null-padded)
//     11  |   1    | reserved (always 0 in OpenMU)
//     12  |   2    | Level (ushort little-endian)
//     14  |   1    | low nibble = Status, high nibble = IsItemBlockActive
//     15  | 18/27  | Appearance binary
//     33/42|   1   | GuildPosition
constexpr std::size_t kCharListHeaderLen   = 4;
constexpr std::size_t kCharEntryClassicLen  = 34;
constexpr std::size_t kCharEntryExtendedLen = 44;
constexpr std::size_t kCharEntryNameOff     = 1;
constexpr std::size_t kCharEntryNameLen     = 10;
constexpr std::size_t kCharEntryLevelOff    = 12;
constexpr std::size_t kCharEntryStatusOff   = 14;
constexpr std::size_t kCharEntryAppearanceOff = 15;
constexpr std::size_t kCharEntryAppearanceClassic  = 18;
constexpr std::size_t kCharEntryAppearanceExtended = 27;

// LoginLongPasswordRequest exact byte layout from OpenMU's
// `src/Network/Packets/ClientToServer/ClientToServerPackets.xml`:
//
//   index | length | field
//   ------+--------+------------------------------------
//       0 |   1    | 0xC3 framing prefix
//       1 |   1    | length byte (0x3C = 60)
//       2 |   1    | head 0xF1
//       3 |   1    | sub  0x01
//       4 |  10    | username  (Xor3-obfuscated ASCII)
//      14 |  20    | password  (Xor3-obfuscated ASCII)
//      34 |   4    | tickCount (uint32 big-endian)
//      38 |   5    | clientVersion (ASCII, e.g. "20404")
//      43 |  16    | clientSerial  (ASCII, e.g. "k1Pk2jcET48mxL3b")
//      59 |   1    | trailing padding byte (zero)
//
// Total = 60 bytes.  Sent as a C3 packet, so the M2 Connection wraps
// it with Xor32 + SimpleModulus on the way to the wire.
constexpr std::size_t kLoginPacketLen      = 60;
constexpr std::size_t kLoginUsernameOff    = 4;
constexpr std::size_t kLoginUsernameLen    = 10;
constexpr std::size_t kLoginPasswordOff    = 14;
constexpr std::size_t kLoginPasswordLen    = 20;
constexpr std::size_t kLoginTickCountOff   = 34;
constexpr std::size_t kLoginVersionOff     = 38;
constexpr std::size_t kLoginVersionLen     = 5;
constexpr std::size_t kLoginSerialOff      = 43;
constexpr std::size_t kLoginSerialLen      = 16;

std::uint16_t read_u16_be(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(p[0]) << 8) | p[1]);
}

void write_u32_be(std::uint8_t* p, std::uint32_t v) noexcept {
    p[0] = static_cast<std::uint8_t>((v >> 24) & 0xFF);
    p[1] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
    p[2] = static_cast<std::uint8_t>((v >>  8) & 0xFF);
    p[3] = static_cast<std::uint8_t>((v >>  0) & 0xFF);
}

std::uint16_t read_u16_le(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(
        p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}

// Extract a null-terminated ASCII string from a fixed-length field.
// Trims trailing NULs and stops at the first one within the slice.
std::string read_fixed_ascii(const std::uint8_t* p, std::size_t len) {
    std::size_t n = 0;
    while (n < len && p[n] != 0) ++n;
    return std::string(reinterpret_cast<const char*>(p), n);
}

int headcode_offset(std::uint8_t prefix) noexcept {
    const int header = header_size_for_prefix(prefix);
    return header > 0 ? header : 0;
}

}  // namespace

const char* GameServerSession::describe_login_result(LoginResult r) noexcept {
    switch (r) {
        case LoginResult::InvalidPassword:        return "invalid password";
        case LoginResult::Okay:                   return "ok";
        case LoginResult::AccountInvalid:         return "account invalid";
        case LoginResult::AccountAlreadyConnected: return "account already connected";
        case LoginResult::ServerIsFull:           return "server is full";
        case LoginResult::AccountBlocked:         return "account blocked";
        case LoginResult::WrongVersion:           return "wrong client version";
        case LoginResult::ConnectionError:        return "connection error";
        case LoginResult::ConnectionClosed3Fails: return "closed after 3 failed attempts";
        case LoginResult::NoChargeInfo:           return "no charge info";
        case LoginResult::SubscriptionTermOver:   return "subscription term over";
        case LoginResult::SubscriptionTimeOver:   return "subscription time over";
        case LoginResult::Unknown:                return "unknown";
    }
    return "unrecognized";
}

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

    if (head == kHeadGameServerEntered && sub == kSubGameServerEntered) {
        parse_entered(data + hc_off + 2, len - (hc_off + 2));
        return;
    }
    if (head == kHeadLogin && sub == kSubLogin) {
        parse_login_response(data + hc_off + 2, len - (hc_off + 2));
        return;
    }
    if (head == kHeadCharacterList && sub == kSubCharacterList) {
        parse_character_list(data + hc_off + 2, len - (hc_off + 2));
        return;
    }

    // Anything else is interesting but not necessarily fatal: keep
    // around for diagnostics.  Future milestones will route these to
    // dedicated handlers (character list, world enter, ...).
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

void GameServerSession::start_login(const std::string& account,
                                    const std::string& password,
                                    const std::uint8_t client_version[5],
                                    const std::uint8_t client_serial[16],
                                    std::uint32_t tick_count) {
    if (phase_ != Phase::Entered) {
        return;
    }

    std::vector<std::uint8_t> pkt(kLoginPacketLen, 0);
    pkt[0] = kPrefixC3;
    pkt[1] = static_cast<std::uint8_t>(kLoginPacketLen);
    pkt[2] = kHeadLogin;
    pkt[3] = kSubLogin;

    // Pad / truncate account into the 10-byte username field and
    // password into the 20-byte password field.  ASCII only here --
    // OpenMU's Xor3Encryptor operates on raw bytes; the client takes
    // the bytes verbatim from the input box.
    const std::size_t acc_n = std::min(account.size(), kLoginUsernameLen);
    std::memcpy(pkt.data() + kLoginUsernameOff, account.data(), acc_n);

    const std::size_t pwd_n = std::min(password.size(), kLoginPasswordLen);
    std::memcpy(pkt.data() + kLoginPasswordOff, password.data(), pwd_n);

    // Xor3-obfuscate the username and password slices in place,
    // matching OpenMU's `Xor3Encryptor(offset=0).Encrypt(span)`.
    xor3_apply(pkt.data() + kLoginUsernameOff, kLoginUsernameLen,
               keys::kXor3.data());
    xor3_apply(pkt.data() + kLoginPasswordOff, kLoginPasswordLen,
               keys::kXor3.data());

    write_u32_be(pkt.data() + kLoginTickCountOff, tick_count);
    std::memcpy(pkt.data() + kLoginVersionOff, client_version, kLoginVersionLen);
    std::memcpy(pkt.data() + kLoginSerialOff,  client_serial,  kLoginSerialLen);
    // Byte 59 stays 0 (trailing padding).

    outbound_.push_back(std::move(pkt));
    phase_ = Phase::LoggingIn;
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

void GameServerSession::start_character_list_request(std::uint8_t language) {
    if (phase_ != Phase::LoggedIn) {
        return;
    }
    std::vector<std::uint8_t> pkt(kCharListReqLen, 0);
    pkt[0] = kPrefixC1;
    pkt[1] = static_cast<std::uint8_t>(kCharListReqLen);
    pkt[2] = kHeadCharacterList;
    pkt[3] = kSubCharacterList;
    pkt[kCharListReqLangOff] = language;
    outbound_.push_back(std::move(pkt));
    phase_ = Phase::CharacterListRequested;
}

void GameServerSession::parse_login_response(const std::uint8_t* body,
                                             std::size_t len) {
    if (len < 1) {
        fail("LoginResponse: payload too short");
        return;
    }
    const std::uint8_t raw = body[0];
    // Map the wire byte to our LoginResult enum -- treat any unknown
    // value as `Unknown` so callers don't have to guard for stray
    // values from a custom server build.
    switch (raw) {
        case 0x00: case 0x01: case 0x02: case 0x03:
        case 0x04: case 0x05: case 0x06: case 0x07:
        case 0x08: case 0x09: case 0x0A: case 0x0B:
            login_result_ = static_cast<LoginResult>(raw);
            break;
        default:
            login_result_ = LoginResult::Unknown;
            break;
    }
    phase_ = (login_result_ == LoginResult::Okay) ? Phase::LoggedIn
                                                  : Phase::LoginFailed;
}

void GameServerSession::parse_character_list(const std::uint8_t* body,
                                             std::size_t len) {
    if (len < kCharListHeaderLen) {
        fail("CharacterList: payload too short for header");
        return;
    }
    const std::uint8_t unlock_flags    = body[0];
    const std::uint8_t move_count      = body[1];
    const std::uint8_t character_count = body[2];
    const bool         vault_extended  = (body[3] != 0);

    const std::size_t entries_bytes = len - kCharListHeaderLen;
    CharacterListVariant variant = CharacterListVariant::Unknown;
    std::size_t entry_size = 0;
    if (character_count == 0) {
        // Empty list -- nothing to disambiguate; mark Classic by
        // convention so callers can switch on it without surprise.
        variant    = CharacterListVariant::Classic;
        entry_size = kCharEntryClassicLen;
    } else if (entries_bytes ==
               character_count * kCharEntryClassicLen) {
        variant    = CharacterListVariant::Classic;
        entry_size = kCharEntryClassicLen;
    } else if (entries_bytes ==
               character_count * kCharEntryExtendedLen) {
        variant    = CharacterListVariant::Extended;
        entry_size = kCharEntryExtendedLen;
    } else {
        char buf[96];
        std::snprintf(buf, sizeof(buf),
                      "CharacterList: %zu entry bytes does not match "
                      "%u * (34 or 44)",
                      entries_bytes,
                      static_cast<unsigned>(character_count));
        fail(buf);
        return;
    }

    std::vector<CharacterEntry> chars;
    chars.reserve(character_count);
    const std::size_t appearance_len =
        (variant == CharacterListVariant::Extended)
            ? kCharEntryAppearanceExtended
            : kCharEntryAppearanceClassic;

    for (std::size_t i = 0; i < character_count; ++i) {
        const std::uint8_t* e = body + kCharListHeaderLen + i * entry_size;
        CharacterEntry ce;
        ce.slot_index = e[0];
        ce.name = read_fixed_ascii(e + kCharEntryNameOff,
                                   kCharEntryNameLen);
        ce.level  = read_u16_le(e + kCharEntryLevelOff);
        const std::uint8_t status_byte = e[kCharEntryStatusOff];
        ce.status            = static_cast<std::uint8_t>(status_byte & 0x0F);
        ce.item_block_active = (status_byte & 0xF0) != 0;
        ce.appearance.assign(e + kCharEntryAppearanceOff,
                             e + kCharEntryAppearanceOff + appearance_len);
        ce.guild_position =
            e[kCharEntryAppearanceOff + appearance_len];
        chars.push_back(std::move(ce));
    }

    unlock_flags_      = unlock_flags;
    move_count_        = move_count;
    is_vault_extended_ = vault_extended;
    character_list_variant_ = variant;
    characters_ = std::move(chars);
    phase_ = Phase::CharacterListReceived;
}

void GameServerSession::fail(std::string msg) {
    error_ = std::move(msg);
    phase_ = Phase::Error;
}

}  // namespace mu::proto
