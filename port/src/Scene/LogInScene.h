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

private:
    void submit_login(App& app);

    // Latched on first submit so a long-held Enter / mashed key
    // doesn't fire multiple LoginLongPasswordRequest packets.  Reset
    // when the scene is re-entered (e.g. after Backspace -> server
    // list -> Enter -> log in).
    bool submitted_ = false;
};

}  // namespace mu
