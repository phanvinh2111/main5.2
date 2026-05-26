#pragma once

#include "Network/TcpClient.h"
#include "Protocol/Connection.h"
#include "Protocol/ConnectServerSession.h"
#include "Render/Renderer.h"
#include "Scene/SceneManager.h"

#include <memory>
#include <optional>
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

    // The ConnectServer session is created lazily when the TCP socket
    // first reaches `Connected`.  Callers (e.g. ServerListScene) must
    // check `connect_server()` for null before reading state.
    const proto::ConnectServerSession* connect_server() const {
        return connect_session_ ? &*connect_session_ : nullptr;
    }

    const std::string& server_host() const { return server_host_; }
    unsigned short     server_port() const { return server_port_; }

private:
    // Drive the ConnectServer state machine forward by one tick:
    // create the session lazily, pump received packets into it, ship
    // any outbound packets it queued.
    void pump_connect_server();

    SDL_Window*    window_   = nullptr;
    Renderer       renderer_;
    SceneManager   scenes_;
    net::TcpClient tcp_;

    // The codec layer is owned by the App so it survives scene
    // transitions.  We construct it as soon as TCP reaches `Connected`,
    // wrapping the (still alive) TcpClient.
    std::optional<proto::Connection>           connection_;
    std::optional<proto::ConnectServerSession> connect_session_;
    bool connect_info_requested_ = false;

    std::string    server_host_;
    unsigned short server_port_ = 0;

    bool   quit_      = false;
    double last_ms_   = 0.0;
};

}  // namespace mu
