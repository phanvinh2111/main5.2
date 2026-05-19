#include "Scene/WebzenScene.h"

#include "App.h"

namespace mu {

void WebzenScene::on_enter(App& /*app*/) { elapsed_ = 0.f; }

void WebzenScene::update(App& app, float dt) {
    elapsed_ += dt;
    if (elapsed_ > 2.0f) {
        app.scenes().set_current(app, SceneId::LogIn);
    }
}

void WebzenScene::render(App& /*app*/, Renderer& r) {
    r.set_clear_color(0, 0, 0, 255);
    r.draw_text(16, 16, "[M1] Scene 1 — WEBZEN (splash placeholder)",
                240, 220, 120);
    r.draw_text(16, 40, "Auto-advances after 2s in M1.");
}

}  // namespace mu
