#include "Scene/ServerListScene.h"

#include "App.h"
#include "Platform/Log.h"

#include <SDL3/SDL.h>

#include <cstdio>

namespace mu {

void ServerListScene::on_enter(App& app) {
    log::info("ServerListScene::on_enter — server target %s:%u",
              app.server_host().c_str(),
              static_cast<unsigned>(app.server_port()));
}

void ServerListScene::on_event(App& app, const SDL_Event& ev) {
    const bool clicked = (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN) ||
                         (ev.type == SDL_EVENT_FINGER_DOWN);
    const bool key_ok  = (ev.type == SDL_EVENT_KEY_DOWN) &&
                         (ev.key.key == SDLK_RETURN ||
                          ev.key.key == SDLK_SPACE);
    if ((clicked || key_ok) &&
        app.tcp().state() == net::TcpState::Connected) {
        app.scenes().set_current(app, SceneId::LogIn);
    }
}

void ServerListScene::update(App& /*app*/, float /*dt*/) {}

void ServerListScene::render(App& app, Renderer& r) {
    r.set_clear_color(8, 12, 28, 255);

    char title[128];
    std::snprintf(title, sizeof(title),
                  "[M1] Scene 0 — SERVER_LIST   target %s:%u",
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
    } else if (app.tcp().state() == net::TcpState::Connected) {
        r.draw_text(16, 64,
                    "Tap / Enter to advance to LOG_IN scene",
                    160, 240, 160);
    }

    r.draw_text(16, r.height() - 24,
                "ESC to quit. Roadmap: port/ROADMAP.md", 140, 140, 140);
}

}  // namespace mu
