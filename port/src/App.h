#pragma once

#include "Network/TcpClient.h"
#include "Render/Renderer.h"
#include "Scene/SceneManager.h"

#include <memory>
#include <string>

struct SDL_Window;
union  SDL_Event;

namespace mu {

class App {
public:
    App();
    ~App();

    bool init();
    void shutdown();

    void on_event(const SDL_Event& ev);

    // Returns false when the app should quit.
    bool tick();

    SceneManager&  scenes()  { return scenes_; }
    Renderer&      renderer(){ return renderer_; }
    net::TcpClient& tcp()    { return tcp_; }

    const std::string& server_host() const { return server_host_; }
    unsigned short     server_port() const { return server_port_; }

private:
    SDL_Window*    window_   = nullptr;
    Renderer       renderer_;
    SceneManager   scenes_;
    net::TcpClient tcp_;

    std::string    server_host_;
    unsigned short server_port_ = 0;

    bool   quit_      = false;
    double last_ms_   = 0.0;
};

}  // namespace mu
