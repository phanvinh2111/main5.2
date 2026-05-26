#include "Scene/LogInScene.h"

#include "App.h"
#include "Platform/Log.h"

#include <SDL3/SDL.h>

#include <cstdio>

namespace mu {

namespace {

const char* phase_label(proto::GameServerSession::Phase p) {
    using P = proto::GameServerSession::Phase;
    switch (p) {
        case P::WaitingForEntered: return "waiting for GameServerEntered";
        case P::Entered:           return "entered";
        case P::Error:             return "error";
    }
    return "?";
}

}  // namespace

void LogInScene::on_enter(App& /*app*/) {
    log::info("LogInScene: M2.6 -- displays GameServer Entered handshake");
}

void LogInScene::on_event(App& app, const SDL_Event& ev) {
    if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_RETURN) {
        // Advancing further requires the encrypted login handshake (M3+).
        // For now the LOG_IN scene is the terminal milestone; we still
        // let Enter advance for skeleton-testing purposes.
        app.scenes().set_current(app, SceneId::Loading);
    }
    if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_BACKSPACE) {
        app.scenes().set_current(app, SceneId::ServerList);
    }
}

void LogInScene::render(App& app, Renderer& r) {
    r.set_clear_color(8, 12, 28, 255);
    r.draw_text(16, 16, "[M2.6] Scene 2 -- LOG_IN  (GameServer handshake)",
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

        if (gs->phase() == proto::GameServerSession::Phase::Entered) {
            char entered[160];
            std::snprintf(entered, sizeof(entered),
                          "Entered: result=0x%02X  playerId=%u  version=\"%s\"",
                          static_cast<unsigned>(gs->result()),
                          static_cast<unsigned>(gs->player_id()),
                          gs->version().c_str());
            r.draw_text(16, 92, entered, 160, 240, 160);
            r.draw_text(16, 112,
                        "Real login UI + LoginLongPasswordRequest packet "
                        "ships in M9 (see ROADMAP.md).",
                        200, 200, 200);
        } else if (gs->phase() == proto::GameServerSession::Phase::Error) {
            r.draw_text(16, 92,
                        "GameServer error: " + gs->last_error(),
                        255, 80, 80);
        }
    }

    r.draw_rect(16, 144, 320, 32, 30, 30, 60, 255);
    r.draw_text(24, 152, "[ accountname     ] (placeholder)", 200, 200, 220);
    r.draw_rect(16, 184, 320, 32, 30, 30, 60, 255);
    r.draw_text(24, 192, "[ password        ] (placeholder)", 200, 200, 220);

    r.draw_text(16, r.height() - 24,
                "Backspace -> back to SERVER_LIST.   "
                "Enter -> Loading (skeleton only).",
                140, 140, 140);
}

}  // namespace mu
