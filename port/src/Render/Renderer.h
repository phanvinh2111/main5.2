#pragma once

#include <cstdint>
#include <string>

struct SDL_Window;
struct SDL_Renderer;

namespace mu {

// M1 renderer: a thin shim over the SDL3 2D `SDL_Renderer` so scenes can
// paint text and quads without dragging in the SDL3 GPU API yet. M3 will
// replace the implementation with a real SDL3 GPU pipeline; the public
// interface here is intentionally narrow so that swap stays mechanical.
class Renderer {
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool init(SDL_Window* win);
    void shutdown();

    void begin_frame();
    void end_frame();

    void set_clear_color(std::uint8_t r, std::uint8_t g,
                         std::uint8_t b, std::uint8_t a);

    // Draw a filled axis-aligned rect with the given RGBA.
    void draw_rect(int x, int y, int w, int h,
                   std::uint8_t r, std::uint8_t g,
                   std::uint8_t b, std::uint8_t a);

    // Draw a single line of debug text using SDL3's built-in debug font.
    void draw_text(int x, int y, const std::string& text,
                   std::uint8_t r = 255, std::uint8_t g = 255,
                   std::uint8_t b = 255, std::uint8_t a = 255);

    int  width()  const { return width_; }
    int  height() const { return height_; }

    SDL_Renderer* raw() const { return sdl_renderer_; }

private:
    SDL_Window*   window_       = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;

    int width_  = 0;
    int height_ = 0;

    std::uint8_t clear_r_ = 10;
    std::uint8_t clear_g_ = 10;
    std::uint8_t clear_b_ = 20;
    std::uint8_t clear_a_ = 255;
};

}  // namespace mu
