#include "Scene/CharacterScene.h"

#include "App.h"
#include "Platform/Log.h"

#include <SDL3/SDL.h>

namespace mu {

void CharacterScene::on_enter(App& /*app*/) {
    log::info("CharacterScene: M1 stub (real port lands in M10)");
}

void CharacterScene::on_event(App& app, const SDL_Event& ev) {
    if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_RETURN) {
        app.scenes().set_current(app, SceneId::Main);
    }
    if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_BACKSPACE) {
        app.scenes().set_current(app, SceneId::LogIn);
    }
}

void CharacterScene::render(App& /*app*/, Renderer& r) {
    r.set_clear_color(20, 8, 28, 255);
    r.draw_text(16, 16, "[M1] Scene 4 — CHARACTER (stub)",
                240, 220, 120);
    r.draw_text(16, 40, "Real port: M10 (CharSelMainWin parity).");

    // Four placeholder character slot rectangles, matching the original's
    // 5-slot grid (the 5th was added in S6 — we'll wire that in M10).
    const int slot_w = 160, slot_h = 240, gap = 24;
    const int total_w = slot_w * 4 + gap * 3;
    const int start_x = (r.width() - total_w) / 2;
    const int y       = 100;
    for (int i = 0; i < 4; ++i) {
        const int x = start_x + i * (slot_w + gap);
        r.draw_rect(x, y, slot_w, slot_h, 40, 30, 50, 255);
        r.draw_text(x + 8, y + 8,
                    std::string("[ slot ") + std::to_string(i) + " ]");
    }

    r.draw_text(16, r.height() - 24,
                "Enter -> MAIN.   Backspace -> LOG_IN.",
                140, 140, 140);
}

}  // namespace mu
