#pragma once

#include "Scene/IScene.h"

namespace mu {

// Mirrors CHARACTER_SCENE = 4 — character select, `CharSelMainWin.cpp`.
class CharacterScene final : public IScene {
public:
    SceneId id() const override { return SceneId::Character; }

    void on_enter(App& app) override;
    void on_event(App& app, const SDL_Event& ev) override;
    void render(App& app, Renderer& r) override;
};

}  // namespace mu
