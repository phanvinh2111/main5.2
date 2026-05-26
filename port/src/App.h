#pragma once

#include "Network/TcpClient.h"
#include "Protocol/Connection.h"
#include "Protocol/ConnectServerSession.h"
#include "Protocol/GameServerSession.h"
#include "Render/Renderer.h"
#include "Scene/SceneManager.h"

#include <cstdint>
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

    // Which protocol the network stack is currently driving.  The app
    // starts in `ConnectServer` -- once the ConnectServer hands out a
    // game-server endpoint and the user advances, the stage flips to
    // `GameServer`.  This mirrors the original Windows client's flow
    // (`SERVER_LIST_SCENE` -> `LOG_IN_SCENE`).
    enum class NetStage {
        ConnectServer,
        GameServer,
    };

    NetStage net_stage() const noexcept { return stage_; }

    // The ConnectServer session is created lazily when the TCP socket
    // first reaches `Connected`.  Callers (e.g. ServerListScene) must
    // check `connect_server()` for null before reading state.
    const proto::ConnectServerSession* connect_server() const {
        return connect_session_ ? &*connect_session_ : nullptr;
    }

    // The GameServer session, populated after start_game_server_dial().
    const proto::GameServerSession* game_server() const {
        return game_session_ ? &*game_session_ : nullptr;
    }

    // Tear down the ConnectServer dialogue, re-dial the TCP socket at
    // the given endpoint, and switch the codec stack to
    // `Codec::GameServer`.  Called by ServerListScene once we have a
    // resolved game-server endpoint.
    //
    // `host` is taken by value on purpose: the caller will typically
    // pass `connect_session_->game_server_host()`, which is a string
    // *owned by* connect_session_ -- and this method destroys
    // connect_session_ before storing the new endpoint, so a reference
    // parameter would dangle the moment we hit `.reset()`.
    void start_game_server_dial(std::string host, std::uint16_t port);

    const std::string& server_host() const { return server_host_; }
    unsigned short     server_port() const { return server_port_; }

private:
    // Drive whichever protocol state machine is currently active by
    // one tick: create the session lazily, pump received packets into
    // it, ship any outbound packets it queued.
    void pump_network();
    void pump_connect_server();
    void pump_game_server();

    SDL_Window*    window_   = nullptr;
    Renderer       renderer_;
    SceneManager   scenes_;
    net::TcpClient tcp_;

    // The codec layer is owned by the App so it survives scene
    // transitions.  We construct it as soon as TCP reaches `Connected`,
    // wrapping the (still alive) TcpClient.
    std::optional<proto::Connection>           connection_;
    std::optional<proto::ConnectServerSession> connect_session_;
    std::optional<proto::GameServerSession>    game_session_;
    NetStage stage_ = NetStage::ConnectServer;
    bool connect_info_requested_ = false;

    std::string    server_host_;
    unsigned short server_port_ = 0;

    bool   quit_      = false;
    double last_ms_   = 0.0;
};

}  // namespace mu
