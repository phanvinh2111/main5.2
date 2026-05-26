#pragma once

#include "Scene/IScene.h"

namespace mu {

// Mirrors `Source Main 5.2/source/Winmain.cpp` boot path
// `SceneFlag = SERVER_LIST_SCENE`. In M1 this is a stub that displays the
// TCP connection status and advances to LogIn when the user taps/clicks.
class ServerListScene final : public IScene {
public:
    SceneId id() const override { return SceneId::ServerList; }

    void on_enter(App& app) override;
    void on_event(App& app, const SDL_Event& ev) override;
    void update(App& app, float dt) override;
    void render(App& app, Renderer& r) override;
};

}  // namespace mu
