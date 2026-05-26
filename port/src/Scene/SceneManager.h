#pragma once

#include "Scene/IScene.h"

#include <array>
#include <memory>

union SDL_Event;

namespace mu {

class App;
class Renderer;

class SceneManager {
public:
    SceneManager();
    ~SceneManager();

    void register_scene(std::unique_ptr<IScene> scene);

    void set_current(App& app, SceneId id);
    SceneId current_id() const { return current_; }
    IScene* current() const;

    void on_event(App& app, const SDL_Event& ev);
    void update(App& app, float dt_seconds);
    void render(App& app, Renderer& r);

private:
    std::array<std::unique_ptr<IScene>, 6> scenes_{};
    SceneId current_ = SceneId::ServerList;
};

}  // namespace mu
