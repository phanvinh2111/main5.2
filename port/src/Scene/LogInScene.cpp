#include "Scene/LogInScene.h"

#include "App.h"
#include "Config.h"
#include "Platform/Log.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mu {

namespace {

const char* phase_label(proto::GameServerSession::Phase p) {
    using P = proto::GameServerSession::Phase;
    switch (p) {
        case P::WaitingForEntered: return "waiting for GameServerEntered";
        case P::Entered:           return "entered (ready to log in)";
        case P::LoggingIn:         return "login request in flight";
        case P::LoggedIn:          return "LOGGED IN";
        case P::LoginFailed:       return "login rejected by server";
        case P::Error:             return "error";
    }
    return "?";
}

// Resolve account/password/version/serial in the same env-var +
// SDL-hint pattern App.cpp uses for the server host.  Account &
// password come from the user; version & serial are protocol-level
// constants copied straight from the original main5.2 Windows client
// (Source Main 5.2/source/WSclient.cpp:108-109).
std::string read_env(const char* var) {
#if defined(MUMOBILE_DESKTOP)
    if (const char* v = std::getenv(var); v && *v) return v;
#else
    (void)var;
#endif
    return {};
}
std::string read_hint(const char* hint) {
    if (const char* h = SDL_GetHint(hint); h && *h) return h;
    return {};
}
std::string resolve(const char* env, const char* hint, const char* fb) {
    if (auto v = read_env(env);  !v.empty()) return v;
    if (auto v = read_hint(hint); !v.empty()) return v;
    return fb;
}

}  // namespace

void LogInScene::on_enter(App& /*app*/) {
    log::info("LogInScene: M2.7 -- LoginLongPasswordRequest handshake");
    submitted_ = false;
}

void LogInScene::on_event(App& app, const SDL_Event& ev) {
    if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_RETURN) {
        submit_login(app);
    }
    if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_BACKSPACE) {
        app.scenes().set_current(app, SceneId::ServerList);
    }
}

void LogInScene::submit_login(App& app) {
    if (submitted_) return;
    auto* gs = app.game_server_mut();
    if (!gs || gs->phase() != proto::GameServerSession::Phase::Entered) {
        log::warn("LogInScene: submit_login called before Entered phase");
        return;
    }

    const std::string account  = resolve("MU_ACCOUNT",  "MU.account",
                                         kDefaultAccount);
    const std::string password = resolve("MU_PASSWORD", "MU.password",
                                         kDefaultPassword);
    const std::string version  = resolve("MU_VERSION",  "MU.version",
                                         kDefaultClientVersion);
    const std::string serial   = resolve("MU_SERIAL",   "MU.serial",
                                         kDefaultClientSerial);

    std::uint8_t version_buf[5] = {};
    std::uint8_t serial_buf[16] = {};
    std::memcpy(version_buf, version.data(),
                std::min<std::size_t>(version.size(), sizeof(version_buf)));
    std::memcpy(serial_buf, serial.data(),
                std::min<std::size_t>(serial.size(), sizeof(serial_buf)));

    log::info("LogInScene: submit login user=\"%s\" version=\"%.*s\"",
              account.c_str(), 5, reinterpret_cast<const char*>(version_buf));
    gs->start_login(account, password, version_buf, serial_buf,
                    SDL_GetTicks());
    submitted_ = true;
}

void LogInScene::render(App& app, Renderer& r) {
    r.set_clear_color(8, 12, 28, 255);
    r.draw_text(16, 16, "[M2.7] Scene 2 -- LOG_IN  (encrypted login handshake)",
                240, 220, 120);

    char tcp_line[160];
    std::snprintf(tcp_line, sizeof(tcp_line), "GameServer dial: %s:%u   tcp=%s",
                  app.server_host().c_str(),
                  static_cast<unsigned>(app.server_port()),
                  net::to_string(app.tcp().state()));
    r.draw_text(16, 40, tcp_line, 180, 220, 255);

    const auto* gs = app.game_server();
    if (!gs) {
        r.draw_text(16, 64,
                    "GameServer: (waiting for TCP re-dial)",
                    180, 180, 220);
    } else {
        char phase_line[128];
        std::snprintf(phase_line, sizeof(phase_line),
                      "GameServer phase: %s", phase_label(gs->phase()));
        r.draw_text(16, 64, phase_line, 180, 220, 255);

        if (gs->phase() != proto::GameServerSession::Phase::WaitingForEntered) {
            char entered[160];
            std::snprintf(entered, sizeof(entered),
                          "Entered: result=0x%02X  playerId=%u  version=\"%s\"",
                          static_cast<unsigned>(gs->result()),
                          static_cast<unsigned>(gs->player_id()),
                          gs->version().c_str());
            r.draw_text(16, 92, entered, 160, 240, 160);
        }

        const auto p = gs->phase();
        using P = proto::GameServerSession::Phase;
        if (p == P::LoggedIn || p == P::LoginFailed) {
            char login_line[160];
            std::snprintf(login_line, sizeof(login_line),
                          "LoginResponse: 0x%02X (%s)",
                          static_cast<unsigned>(gs->login_result()),
                          proto::GameServerSession::describe_login_result(
                              gs->login_result()));
            const std::uint8_t cr = p == P::LoggedIn ? 120 : 255;
            const std::uint8_t cg = p == P::LoggedIn ? 240 : 100;
            const std::uint8_t cb = p == P::LoggedIn ? 120 : 100;
            r.draw_text(16, 112, login_line, cr, cg, cb);
            r.draw_text(16, 132,
                        "Character list + world enter ship in M4+ "
                        "(see ROADMAP.md).",
                        200, 200, 200);
        } else if (p == P::Error) {
            r.draw_text(16, 112,
                        "GameServer error: " + gs->last_error(),
                        255, 80, 80);
        } else if (p == P::Entered && !submitted_) {
            r.draw_text(16, 112,
                        "Press Enter to send LoginLongPasswordRequest "
                        "(F1 01) ...",
                        200, 220, 140);
        } else if (p == P::LoggingIn) {
            r.draw_text(16, 112,
                        "Login request sent, waiting for server response ...",
                        200, 220, 140);
        }
    }

    r.draw_rect(16, 164, 320, 32, 30, 30, 60, 255);
    r.draw_text(24, 172, "[ accountname     ] (placeholder)", 200, 200, 220);
    r.draw_rect(16, 204, 320, 32, 30, 30, 60, 255);
    r.draw_text(24, 212, "[ password        ] (placeholder)", 200, 200, 220);

    r.draw_text(16, r.height() - 24,
                "Backspace -> back to SERVER_LIST.   "
                "Enter -> submit login (uses MU_ACCOUNT / MU_PASSWORD env).",
                140, 140, 140);
}

}  // namespace mu
