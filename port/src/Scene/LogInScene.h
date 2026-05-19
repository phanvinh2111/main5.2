#pragma once

#include "Scene/IScene.h"

namespace mu {

// Mirrors LOG_IN_SCENE = 2 — `LoginWin.cpp` in the original client.
class LogInScene final : public IScene {
public:
    SceneId id() const override { return SceneId::LogIn; }

    void on_enter(App& app) override;
    void on_event(App& app, const SDL_Event& ev) override;
    void render(App& app, Renderer& r) override;
};

}  // namespace mu
