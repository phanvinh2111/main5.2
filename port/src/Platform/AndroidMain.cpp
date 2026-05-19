// Android entry point. SDL3's Java SDLActivity dlopens libmain.so and calls
// SDL_main(); SDL3's main-callbacks macro takes care of the bridge.

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

#include "App.h"

#include <new>

namespace {
mu::App* g_app = nullptr;
}

extern "C" SDL_AppResult SDL_AppInit(void** appstate, int /*argc*/,
                                     char* /*argv*/[]) {
    g_app = new (std::nothrow) mu::App();
    if (!g_app || !g_app->init()) {
        delete g_app;
        g_app = nullptr;
        return SDL_APP_FAILURE;
    }
    *appstate = g_app;
    return SDL_APP_CONTINUE;
}

extern "C" SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* app = static_cast<mu::App*>(appstate);
    if (app && event) app->on_event(*event);
    return SDL_APP_CONTINUE;
}

extern "C" SDL_AppResult SDL_AppIterate(void* appstate) {
    auto* app = static_cast<mu::App*>(appstate);
    if (!app) return SDL_APP_FAILURE;
    return app->tick() ? SDL_APP_CONTINUE : SDL_APP_SUCCESS;
}

extern "C" void SDL_AppQuit(void* appstate, SDL_AppResult /*result*/) {
    auto* app = static_cast<mu::App*>(appstate);
    if (app) {
        app->shutdown();
        delete app;
    }
    g_app = nullptr;
}
