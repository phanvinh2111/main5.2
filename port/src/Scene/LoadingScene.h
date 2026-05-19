#pragma once

#include "Scene/IScene.h"

namespace mu {

// Mirrors LOADING_SCENE = 3 — `LoadingScene.cpp` in the original client.
class LoadingScene final : public IScene {
public:
    SceneId id() const override { return SceneId::Loading; }

    void on_enter(App& app) override;
    void update(App& app, float dt) override;
    void render(App& app, Renderer& r) override;

private:
    float elapsed_ = 0.f;
};

}  // namespace mu
