#include "App.h"

#include "Config.h"
#include "Platform/Log.h"
#include "Scene/CharacterScene.h"
#include "Scene/LoadingScene.h"
#include "Scene/LogInScene.h"
#include "Scene/MainScene.h"
#include "Scene/ServerListScene.h"
#include "Scene/WebzenScene.h"

#include <SDL3/SDL.h>

#include <cstdlib>
#include <memory>
#include <string>

namespace mu {

App::App()  = default;
App::~App() { shutdown(); }

namespace {

std::string env_or_default(const char* key, const char* fallback) {
#if defined(MUMOBILE_DESKTOP)
    if (const char* v = std::getenv(key); v && *v) return v;
#else
    (void)key;
#endif
    // SDL hints work on every platform — Android can set them via
    // SDL_SetHint() from Java, iOS via Info.plist preferences.
    if (const char* h = SDL_GetHint(key); h && *h) return h;
    return fallback;
}

unsigned short env_or_default_port(const char* key, unsigned short fb) {
    auto s = env_or_default(key, "");
    if (s.empty()) return fb;
    long v = std::strtol(s.c_str(), nullptr, 10);
    if (v <= 0 || v > 65535) return fb;
    return static_cast<unsigned short>(v);
}

}  // namespace

bool App::init() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        log::error("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    window_ = SDL_CreateWindow(kAppTitle, kWinWidth, kWinHeight,
                               SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window_) {
        log::error("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    if (!renderer_.init(window_)) {
        return false;
    }

    server_host_ = env_or_default("MU_SERVER_HOST", kDefaultServerHost);
    server_port_ = env_or_default_port("MU_SERVER_PORT", kDefaultServerPort);
    log::info("App: target server = %s:%u", server_host_.c_str(),
              static_cast<unsigned>(server_port_));

    scenes_.register_scene(std::make_unique<ServerListScene>());
    scenes_.register_scene(std::make_unique<WebzenScene>());
    scenes_.register_scene(std::make_unique<LogInScene>());
    scenes_.register_scene(std::make_unique<LoadingScene>());
    scenes_.register_scene(std::make_unique<CharacterScene>());
    scenes_.register_scene(std::make_unique<MainScene>());

    // Mirror the original boot order from `Winmain.cpp::main()` which kicks
    // off with `SceneFlag = SERVER_LIST_SCENE`.
    scenes_.set_current(*this, SceneId::ServerList);

    // Kick the TCP dial straight away so M1 can prove the server is reachable.
    tcp_.connect(server_host_, server_port_);

    last_ms_ = static_cast<double>(SDL_GetTicks());
    return true;
}

void App::shutdown() {
    tcp_.disconnect();
    renderer_.shutdown();
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

void App::on_event(const SDL_Event& ev) {
    if (ev.type == SDL_EVENT_QUIT) {
        quit_ = true;
        return;
    }
    if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_ESCAPE) {
        quit_ = true;
        return;
    }
    scenes_.on_event(*this, ev);
}

bool App::tick() {
    if (quit_) return false;

    const double now_ms = static_cast<double>(SDL_GetTicks());
    const float  dt     = static_cast<float>((now_ms - last_ms_) / 1000.0);
    last_ms_            = now_ms;

    scenes_.update(*this, dt);

    renderer_.begin_frame();
    scenes_.render(*this, renderer_);
    renderer_.end_frame();
    return true;
}

}  // namespace mu
