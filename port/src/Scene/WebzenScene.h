#pragma once

#include "Scene/IScene.h"

namespace mu {

// Mirrors WEBZEN_SCENE = 1. In the original client this is the intro/splash
// rendered by `MainSplash.cpp`. M1 stub auto-advances after a short timer.
class WebzenScene final : public IScene {
public:
    SceneId id() const override { return SceneId::Webzen; }

    void on_enter(App& app) override;
    void update(App& app, float dt) override;
    void render(App& app, Renderer& r) override;

private:
    float elapsed_ = 0.f;
};

}  // namespace mu
