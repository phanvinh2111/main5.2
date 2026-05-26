#include "Scene/LoadingScene.h"

#include "App.h"

namespace mu {

void LoadingScene::on_enter(App& /*app*/) { elapsed_ = 0.f; }

void LoadingScene::update(App& app, float dt) {
    elapsed_ += dt;
    if (elapsed_ > 1.5f) {
        app.scenes().set_current(app, SceneId::Character);
    }
}

void LoadingScene::render(App& /*app*/, Renderer& r) {
    r.set_clear_color(0, 0, 0, 255);
    r.draw_text(16, 16, "[M1] Scene 3 — LOADING (stub)", 240, 220, 120);

    const int bar_w = 480;
    const int bar_h = 16;
    const int bx    = (r.width()  - bar_w) / 2;
    const int by    = (r.height() - bar_h) / 2;
    r.draw_rect(bx, by, bar_w, bar_h, 40, 40, 40, 255);

    const float t = elapsed_ / 1.5f;
    const int   fill = static_cast<int>(static_cast<float>(bar_w) *
                                        (t < 1.f ? t : 1.f));
    r.draw_rect(bx, by, fill, bar_h, 200, 160, 60, 255);
}

}  // namespace mu
