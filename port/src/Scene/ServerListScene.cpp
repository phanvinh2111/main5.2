#include "Scene/ServerListScene.h"

#include "App.h"
#include "Platform/Log.h"

#include <SDL3/SDL.h>

#include <cstdio>

namespace mu {

namespace {

const char* phase_label(proto::ConnectServerSession::Phase p) {
    using P = proto::ConnectServerSession::Phase;
    switch (p) {
        case P::WaitingForHello:          return "waiting for hello";
        case P::RequestedServerList:      return "requested server list";
        case P::ServerListReceived:       return "server list received";
        case P::RequestedConnectionInfo:  return "requested connection info";
        case P::ConnectionInfoReceived:   return "connection info received";
        case P::Error:                    return "error";
    }
    return "?";
}

}  // namespace

void ServerListScene::on_enter(App& app) {
    log::info("ServerListScene::on_enter -- server target %s:%u",
              app.server_host().c_str(),
              static_cast<unsigned>(app.server_port()));
}

void ServerListScene::on_event(App& app, const SDL_Event& ev) {
    const bool clicked = (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN) ||
                         (ev.type == SDL_EVENT_FINGER_DOWN);
    const bool key_ok  = (ev.type == SDL_EVENT_KEY_DOWN) &&
                         (ev.key.key == SDLK_RETURN ||
                          ev.key.key == SDLK_SPACE);
    // We require the ConnectServer to have actually told us a game
    // server endpoint before letting the player advance.  That's the
    // M2.5 promotion criterion -- M1's pure-TCP-reachable was too lax.
    const auto* cs = app.connect_server();
    const bool ready =
        cs &&
        cs->phase() ==
            proto::ConnectServerSession::Phase::ConnectionInfoReceived;
    if ((clicked || key_ok) && ready) {
        // Hand off to the GameServer using the endpoint the ConnectServer
        // resolved.  This tears down the Plain Connection and re-dials the
        // TCP socket at the new host:port with Codec::GameServer.
        app.start_game_server_dial(cs->game_server_host(),
                                   cs->game_server_port());
        app.scenes().set_current(app, SceneId::LogIn);
    }
}

void ServerListScene::update(App& /*app*/, float /*dt*/) {}

void ServerListScene::render(App& app, Renderer& r) {
    r.set_clear_color(8, 12, 28, 255);

    char title[160];
    std::snprintf(title, sizeof(title),
                  "[M2.5] Scene 0 -- SERVER_LIST   target %s:%u",
                  app.server_host().c_str(),
                  static_cast<unsigned>(app.server_port()));
    r.draw_text(16, 16, title, 240, 220, 120);

    char status[128];
    std::snprintf(status, sizeof(status), "TCP: %s",
                  net::to_string(app.tcp().state()));
    r.draw_text(16, 40, status);

    if (app.tcp().state() == net::TcpState::Failed) {
        r.draw_text(16, 64, "error: " + app.tcp().last_error(),
                    255, 80, 80);
        return;
    }

    const auto* cs = app.connect_server();
    if (!cs) {
        r.draw_text(16, 64, "ConnectServer: (waiting for TCP)",
                    180, 180, 220);
        return;
    }

    char phase_line[128];
    std::snprintf(phase_line, sizeof(phase_line),
                  "ConnectServer phase: %s",
                  phase_label(cs->phase()));
    r.draw_text(16, 64, phase_line, 180, 220, 255);

    int y = 92;
    const auto& list = cs->server_list();
    if (!list.empty()) {
        char header[64];
        std::snprintf(header, sizeof(header),
                      "Servers (%zu):", list.size());
        r.draw_text(16, y, header, 200, 200, 200);
        y += 20;
        for (const auto& e : list) {
            char line[80];
            std::snprintf(line, sizeof(line),
                          "  id=%u  load=%u%%",
                          static_cast<unsigned>(e.id),
                          static_cast<unsigned>(e.load_percent));
            r.draw_text(32, y, line);
            y += 18;
        }
    }

    if (cs->phase() ==
        proto::ConnectServerSession::Phase::ConnectionInfoReceived) {
        char gs[128];
        std::snprintf(gs, sizeof(gs),
                      "Game server endpoint: %s:%u",
                      cs->game_server_host().c_str(),
                      static_cast<unsigned>(cs->game_server_port()));
        r.draw_text(16, y, gs, 160, 240, 160);
        y += 22;
        r.draw_text(16, y, "Tap / Enter to advance to LOG_IN scene",
                    160, 240, 160);
    } else if (cs->phase() ==
               proto::ConnectServerSession::Phase::Error) {
        r.draw_text(16, y, "ConnectServer error: " + cs->last_error(),
                    255, 80, 80);
    }

    r.draw_text(16, r.height() - 24,
                "ESC to quit. Roadmap: port/ROADMAP.md", 140, 140, 140);
}

}  // namespace mu
