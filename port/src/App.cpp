#include "App.h"

#include "Config.h"
#include "Platform/Log.h"
#include "Platform/PlatformBootstrap.h"
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

// Naming convention:
//   * Environment variables use C-identifier names (MU_SERVER_HOST). Only
//     consulted on desktop, where std::getenv is meaningful.
//   * SDL hint names use the dotted MU.server.host form, matching the keys
//     in port/platform/ios/Info.plist and the README. SDL_GetHint does NOT
//     read Info.plist by itself; the iOS bootstrap in platform::bootstrap
//     copies the relevant Info.plist entries into SDL hints via
//     SDL_SetHint() during App::init().
namespace {

std::string read_env(const char* var) {
#if defined(MUMOBILE_DESKTOP)
    if (const char* v = std::getenv(var); v && *v) return v;
#else
    (void)var;
#endif
    return {};
}

std::string read_hint(const char* hint) {
    if (const char* h = SDL_GetHint(hint); h && *h) return h;
    return {};
}

std::string resolve_string(const char* env_var, const char* hint_name,
                           const char* fallback) {
    if (auto v = read_env(env_var);  !v.empty()) return v;
    if (auto v = read_hint(hint_name); !v.empty()) return v;
    return fallback;
}

unsigned short resolve_port(const char* env_var, const char* hint_name,
                            unsigned short fb) {
    auto s = resolve_string(env_var, hint_name, "");
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

    // Per-platform startup work that has to run before we query SDL hints:
    // on iOS this copies the Info.plist server-override entries into SDL
    // hints. On other platforms it's a no-op (M2 will add the Android Java
    // counterpart).
    platform::bootstrap();

    window_ = SDL_CreateWindow(kAppTitle, kWinWidth, kWinHeight,
                               SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window_) {
        log::error("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    if (!renderer_.init(window_)) {
        return false;
    }

    server_host_ = resolve_string("MU_SERVER_HOST", "MU.server.host",
                                  kDefaultServerHost);
    server_port_ = resolve_port("MU_SERVER_PORT", "MU.server.port",
                                kDefaultServerPort);
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
