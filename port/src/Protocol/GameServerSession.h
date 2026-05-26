#pragma once

// State machine for talking to an OpenMU GameServer (port 55901 on
// the reference server).  The client dials the GameServer after the
// ConnectServer has handed out its endpoint (see
// `ConnectServerSession`).
//
// Inbound protocol:
//   1. Server greets with a plain `C1 F1 00` "GameServerEntered"
//      frame (M2.6 parses this -> Phase::Entered):
//        C1 0C F1 00 <result:1> <playerId:2 BE> <version:5 ASCII>
//   2. After we ship a LoginLongPassword request, server replies with
//      a plain `C1 F1 01` "LoginResponse" frame (M2.7 -> Phase::LoggedIn
//      or Phase::LoginFailed):
//        C1 05 F1 01 <result:1>
//
// Outbound (M2.7):
//   * LoginLongPasswordRequest (C3 F1 01, 60-byte payload, Xor3-
//     obfuscated username/password + plaintext tick/version/serial).
//     Encrypted on the wire by `Connection(Codec::GameServer)` via
//     Xor32 + SimpleModulus.
//
// Inbound + outbound (M2.8):
//   * RequestCharacterList  (C1 F3 00, 5 bytes, plaintext): sent right
//     after LoginResponse=Okay. One trailing byte selects the client
//     language (OpenMU just echoes it).
//   * CharacterList         (C1 F3 00, variable length): server's reply.
//     Header is `UnlockFlags(1) MoveCnt(1) CharacterCount(N) IsVaultExtended(1)`
//     followed by N character structures. OpenMU emits two variants
//     depending on the server's configured Season -- classic (34B per
//     character, 18B appearance) or extended (44B per character, 27B
//     appearance). We auto-detect from the remaining payload length and
//     populate Phase::CharacterListReceived either way.
//
// The C1 framing prefix means the LoginResponse is *not* encrypted on
// the wire (both SimpleModulus and Xor32 pass C1/C2 through). The
// LoginLongPasswordRequest IS encrypted (C3 prefix) and exercises the
// full M2 codec stack against a real OpenMU server.

#include <cstdint>
#include <string>
#include <vector>

namespace mu::proto {

class GameServerSession {
public:
    enum class Phase {
        WaitingForEntered,
        Entered,
        LoggingIn,
        LoggedIn,
        LoginFailed,
        CharacterListRequested,
        CharacterListReceived,
        Error,
    };

    // Layout variant of the F3 00 CharacterList packet.
    //   * Classic   -- Season 1..5, 34 bytes/char, 18B appearance.
    //   * Extended  -- Season 6+,    44 bytes/char, 27B appearance.
    // Determined at parse time by checking the remaining payload
    // length against `4 + N * 34` or `4 + N * 44`.
    enum class CharacterListVariant : std::uint8_t {
        Unknown  = 0,
        Classic  = 34,
        Extended = 44,
    };

    // One row in the character selection list.  Mirrors OpenMU's
    // `CharacterData` structure from
    // `ServerToClient/ServerToClientPackets.xml` -- we keep only the
    // fields the LOG_IN scene needs to render.  `appearance` is the
    // raw 18 or 27 bytes from the wire (interpreted by M6+ later).
    struct CharacterEntry {
        std::uint8_t slot_index = 0;
        std::string  name;
        std::uint16_t level = 0;
        std::uint8_t status = 0;       // low nibble of byte 14
        bool         item_block_active = false; // high nibble of byte 14
        std::vector<std::uint8_t> appearance;
        std::uint8_t guild_position = 0;
    };

    // OpenMU `LoginResult` enum from
    // `src/Network/Packets/ServerToClient/ServerToClientPackets.xml`.
    // The wire byte is one of these values; we keep them as named
    // constants so the LOG_IN scene can show a human-readable reason.
    enum class LoginResult : std::uint8_t {
        InvalidPassword        = 0x00,
        Okay                   = 0x01,
        AccountInvalid         = 0x02,
        AccountAlreadyConnected = 0x03,
        ServerIsFull           = 0x04,
        AccountBlocked         = 0x05,
        WrongVersion           = 0x06,
        ConnectionError        = 0x07,
        ConnectionClosed3Fails = 0x08,
        NoChargeInfo           = 0x09,
        SubscriptionTermOver   = 0x0A,
        SubscriptionTimeOver   = 0x0B,
        Unknown                = 0xFF,
    };

    static const char* describe_login_result(LoginResult r) noexcept;

    // Process one fully-framed inbound packet.
    void on_packet(const std::uint8_t* data, std::size_t len);

    // Outbound queue.  Drained by the App into the wire each tick.
    std::vector<std::vector<std::uint8_t>> take_outbound();

    // Queue a LoginLongPasswordRequest packet.  Valid only when
    // phase() == Phase::Entered.  No-op otherwise.
    //
    //   * account       — ASCII, max 10 bytes, padded with NULs.
    //   * password      — ASCII, max 20 bytes, padded with NULs.
    //   * client_version — exactly 5 bytes ASCII (e.g. "20404").
    //   * client_serial  — exactly 16 bytes ASCII (e.g.
    //                      "k1Pk2jcET48mxL3b").
    //   * tick_count    — value of an arbitrary monotonic counter,
    //                      sent big-endian.  The server records it but
    //                      typically does not reject based on its value.
    //
    // The username & password are Xor3-obfuscated before they hit the
    // wire, matching OpenMU's `Xor3Encryptor(offset=0)`.  The full
    // 60-byte packet is then handed back to the App as a `C3 F1 01`
    // frame; the App's `Connection(Codec::GameServer)` adds the Xor32
    // and SimpleModulus layers.
    void start_login(const std::string& account,
                     const std::string& password,
                     const std::uint8_t client_version[5],
                     const std::uint8_t client_serial[16],
                     std::uint32_t tick_count);

    // Queue a 5-byte `C1 05 F3 00 <language>` RequestCharacterList
    // packet.  Valid only when phase() == Phase::LoggedIn -- a no-op
    // otherwise, so callers may invoke it unconditionally after
    // observing `Phase::LoggedIn`.  `language` is forwarded verbatim
    // (OpenMU does not gate on its value; the original client passes
    // 0).
    void start_character_list_request(std::uint8_t language = 0);

    Phase phase() const noexcept { return phase_; }

    // GameServerEntered fields (populated after Phase::Entered).
    std::uint8_t  result() const noexcept       { return result_; }
    std::uint16_t player_id() const noexcept    { return player_id_; }
    const std::string& version() const noexcept { return version_; }

    // LoginResponse fields (populated after Phase::LoggedIn or
    // Phase::LoginFailed).
    LoginResult login_result() const noexcept    { return login_result_; }

    // CharacterList fields (populated after Phase::CharacterListReceived).
    CharacterListVariant character_list_variant() const noexcept {
        return character_list_variant_;
    }
    std::uint8_t unlock_flags()    const noexcept { return unlock_flags_; }
    std::uint8_t move_count()      const noexcept { return move_count_; }
    bool         is_vault_extended() const noexcept { return is_vault_extended_; }
    const std::vector<CharacterEntry>& characters() const noexcept {
        return characters_;
    }

    const std::string& last_error() const noexcept { return error_; }
    const std::string& last_unknown() const noexcept { return last_unknown_; }

private:
    void parse_entered(const std::uint8_t* body, std::size_t len);
    void parse_login_response(const std::uint8_t* body, std::size_t len);
    void parse_character_list(const std::uint8_t* body, std::size_t len);
    void fail(std::string msg);

    Phase phase_ = Phase::WaitingForEntered;
    std::vector<std::vector<std::uint8_t>> outbound_;
    std::uint8_t  result_ = 0;
    std::uint16_t player_id_ = 0;
    std::string   version_;
    LoginResult   login_result_ = LoginResult::Unknown;
    CharacterListVariant character_list_variant_ =
        CharacterListVariant::Unknown;
    std::uint8_t  unlock_flags_      = 0;
    std::uint8_t  move_count_        = 0;
    bool          is_vault_extended_ = false;
    std::vector<CharacterEntry> characters_;
    std::string   error_;
    std::string   last_unknown_;
};

}  // namespace mu::proto
