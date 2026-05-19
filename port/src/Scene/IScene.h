#pragma once

union SDL_Event;

namespace mu {

class App;       // forward
class Renderer;  // forward

// Mirrors EGameScene from `Source Main 5.2/source/_define.h`:
//   SERVER_LIST_SCENE = 0, WEBZEN_SCENE = 1, LOG_IN_SCENE = 2,
//   LOADING_SCENE = 3, CHARACTER_SCENE = 4, MAIN_SCENE = 5
enum class SceneId {
    ServerList = 0,
    Webzen     = 1,
    LogIn      = 2,
    Loading    = 3,
    Character  = 4,
    Main       = 5,
};

const char* to_string(SceneId id);

class IScene {
public:
    virtual ~IScene() = default;

    virtual SceneId id() const = 0;

    virtual void on_enter(App& app) {}
    virtual void on_exit(App& app) {}
    virtual void on_event(App& app, const SDL_Event& ev) {}

    // Called once per frame with the elapsed delta in seconds.
    virtual void update(App& app, float dt_seconds) {}

    // Called after update; the renderer is bound to the back buffer.
    virtual void render(App& app, Renderer& r) = 0;
};

}  // namespace mu
