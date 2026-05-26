#include "Scene/MainScene.h"

#include "App.h"
#include "Platform/Log.h"

#include <SDL3/SDL.h>

namespace mu {

void MainScene::on_enter(App& /*app*/) {
    log::info("MainScene: M1 stub. M13-M16 implement the real in-game scene");
}

void MainScene::on_event(App& app, const SDL_Event& ev) {
    if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_BACKSPACE) {
        app.scenes().set_current(app, SceneId::Character);
    }
}

void MainScene::render(App& /*app*/, Renderer& r) {
    r.set_clear_color(8, 28, 12, 255);
    r.draw_text(16, 16, "[M1] Scene 5 — MAIN (stub)",
                240, 220, 120);
    r.draw_text(16, 40,
                "Real port: M13 (camera+input), M14 (HUD), "
                "M15 (combat), M16 (MU Helper).");
    r.draw_text(16, 64, "Backspace -> CHARACTER.", 140, 140, 140);
}

}  // namespace mu
