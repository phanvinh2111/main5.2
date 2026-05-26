#include "Scene/SceneManager.h"

#include "Platform/Log.h"

namespace mu {

const char* to_string(SceneId id) {
    switch (id) {
        case SceneId::ServerList: return "ServerList";
        case SceneId::Webzen:     return "Webzen";
        case SceneId::LogIn:      return "LogIn";
        case SceneId::Loading:    return "Loading";
        case SceneId::Character:  return "Character";
        case SceneId::Main:       return "Main";
    }
    return "?";
}

SceneManager::SceneManager()  = default;
SceneManager::~SceneManager() = default;

void SceneManager::register_scene(std::unique_ptr<IScene> scene) {
    if (!scene) return;
    const auto idx = static_cast<std::size_t>(scene->id());
    if (idx >= scenes_.size()) return;
    scenes_[idx] = std::move(scene);
}

IScene* SceneManager::current() const {
    const auto idx = static_cast<std::size_t>(current_);
    if (idx >= scenes_.size()) return nullptr;
    return scenes_[idx].get();
}

void SceneManager::set_current(App& app, SceneId id) {
    if (auto* cur = current(); cur) {
        cur->on_exit(app);
    }
    current_ = id;
    log::info("SceneManager: -> %s", to_string(id));
    if (auto* cur = current(); cur) {
        cur->on_enter(app);
    }
}

void SceneManager::on_event(App& app, const SDL_Event& ev) {
    if (auto* cur = current(); cur) cur->on_event(app, ev);
}

void SceneManager::update(App& app, float dt) {
    if (auto* cur = current(); cur) cur->update(app, dt);
}

void SceneManager::render(App& app, Renderer& r) {
    if (auto* cur = current(); cur) cur->render(app, r);
}

}  // namespace mu
