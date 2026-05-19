#include "Render/Renderer.h"

#include "Platform/Log.h"

#include <SDL3/SDL.h>

namespace mu {

Renderer::Renderer()  = default;
Renderer::~Renderer() { shutdown(); }

bool Renderer::init(SDL_Window* win) {
    window_ = win;
    sdl_renderer_ = SDL_CreateRenderer(win, nullptr);
    if (!sdl_renderer_) {
        log::error("SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }
    SDL_GetWindowSize(win, &width_, &height_);
    log::info("Renderer: %dx%d (M1 placeholder SDL_Renderer)",
              width_, height_);
    return true;
}

void Renderer::shutdown() {
    if (sdl_renderer_) {
        SDL_DestroyRenderer(sdl_renderer_);
        sdl_renderer_ = nullptr;
    }
}

void Renderer::begin_frame() {
    if (!sdl_renderer_) return;
    SDL_GetWindowSize(window_, &width_, &height_);
    SDL_SetRenderDrawColor(sdl_renderer_, clear_r_, clear_g_, clear_b_,
                           clear_a_);
    SDL_RenderClear(sdl_renderer_);
}

void Renderer::end_frame() {
    if (!sdl_renderer_) return;
    SDL_RenderPresent(sdl_renderer_);
}

void Renderer::set_clear_color(std::uint8_t r, std::uint8_t g,
                               std::uint8_t b, std::uint8_t a) {
    clear_r_ = r; clear_g_ = g; clear_b_ = b; clear_a_ = a;
}

void Renderer::draw_rect(int x, int y, int w, int h,
                         std::uint8_t r, std::uint8_t g,
                         std::uint8_t b, std::uint8_t a) {
    if (!sdl_renderer_) return;
    SDL_FRect rc{static_cast<float>(x), static_cast<float>(y),
                 static_cast<float>(w), static_cast<float>(h)};
    SDL_SetRenderDrawColor(sdl_renderer_, r, g, b, a);
    SDL_RenderFillRect(sdl_renderer_, &rc);
}

void Renderer::draw_text(int x, int y, const std::string& text,
                         std::uint8_t r, std::uint8_t g,
                         std::uint8_t b, std::uint8_t a) {
    if (!sdl_renderer_) return;
    SDL_SetRenderDrawColor(sdl_renderer_, r, g, b, a);
    SDL_RenderDebugText(sdl_renderer_, static_cast<float>(x),
                        static_cast<float>(y), text.c_str());
}

}  // namespace mu
