#include "Scene/LogInScene.h"

#include "App.h"
#include "Platform/Log.h"

#include <SDL3/SDL.h>

namespace mu {

void LogInScene::on_enter(App& /*app*/) {
    log::info("LogInScene: M1 stub — M9 will implement the real packet flow");
}

void LogInScene::on_event(App& app, const SDL_Event& ev) {
    if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_RETURN) {
        app.scenes().set_current(app, SceneId::Loading);
    }
    if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_BACKSPACE) {
        app.scenes().set_current(app, SceneId::ServerList);
    }
}

void LogInScene::render(App& app, Renderer& r) {
    r.set_clear_color(8, 12, 28, 255);
    r.draw_text(16, 16, "[M1] Scene 2 — LOG_IN (stub)",
                240, 220, 120);
    r.draw_text(16, 40, "Real login UI ships in M9 — see ROADMAP.md.");
    r.draw_text(16, 64, "Enter -> advance to Loading.   Backspace -> back.");
    r.draw_rect(16, 96, 320, 32, 30, 30, 60, 255);
    r.draw_text(24, 104, "[ accountname     ] (placeholder)", 200, 200, 220);
    r.draw_rect(16, 136, 320, 32, 30, 30, 60, 255);
    r.draw_text(24, 144, "[ password        ] (placeholder)", 200, 200, 220);

    r.draw_text(16, r.height() - 24,
                std::string("server ") + app.server_host() + ":" +
                std::to_string(app.server_port()) + "  tcp=" +
                net::to_string(app.tcp().state()),
                140, 140, 140);
}

}  // namespace mu
