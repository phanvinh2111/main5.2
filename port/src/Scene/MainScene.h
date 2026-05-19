#pragma once

#include "Scene/IScene.h"

namespace mu {

// Mirrors MAIN_SCENE = 5 — in-game world. The original implementation lives
// across `ZzzScene.cpp`, `MapManager.cpp`, all the `GM*.cpp` map files,
// `NewUIMainFrameWindow.cpp`, etc. M1 is a stub.
class MainScene final : public IScene {
public:
    SceneId id() const override { return SceneId::Main; }

    void on_enter(App& app) override;
    void on_event(App& app, const SDL_Event& ev) override;
    void render(App& app, Renderer& r) override;
};

}  // namespace mu
