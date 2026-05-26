#include "App.h"

#include "Config.h"
#include "Platform/Log.h"
#include "Platform/PlatformBootstrap.h"
#include "Protocol/Framing.h"
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
    // Tear down the codec layer BEFORE the TcpClient, since the
    // Connection holds a reference back into tcp_.
    connect_session_.reset();
    game_session_.reset();
    connection_.reset();
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

namespace {

void log_packet(const char* dir, const std::vector<std::uint8_t>& p) {
    if (p.empty()) return;
    const int hdr = proto::header_size_for_prefix(p[0]);
    const std::uint8_t head = (hdr > 0 && p.size() > std::size_t(hdr)) ?
                              p[hdr] : 0;
    const std::uint8_t sub  = (hdr > 0 && p.size() > std::size_t(hdr + 1)) ?
                              p[hdr + 1] : 0;
    log::info("App: %s %zu byte packet, head=0x%02X sub=0x%02X",
              dir, p.size(),
              static_cast<unsigned>(head),
              static_cast<unsigned>(sub));
}

}  // namespace

void App::pump_network() {
    switch (stage_) {
        case NetStage::ConnectServer: pump_connect_server(); break;
        case NetStage::GameServer:    pump_game_server();    break;
    }
}

void App::start_game_server_dial(const std::string& host,
                                 std::uint16_t port) {
    if (stage_ == NetStage::GameServer) {
        log::warn("App: start_game_server_dial called twice, ignoring");
        return;
    }
    log::info("App: switching ConnectServer -> GameServer dial %s:%u",
              host.c_str(), static_cast<unsigned>(port));

    // Tear down ConnectServer codec stack FIRST.  Connection holds a
    // reference to tcp_, so its destructor must run before we touch
    // the underlying socket worker.
    connect_session_.reset();
    connection_.reset();
    connect_info_requested_ = false;

    tcp_.disconnect();
    server_host_ = host;
    server_port_ = port;
    tcp_.connect(server_host_, server_port_);

    stage_ = NetStage::GameServer;
}

void App::pump_connect_server() {
    if (tcp_.state() != net::TcpState::Connected) {
        return;
    }
    // Lazy init on first reach of `Connected`.  Port 44405 is the
    // OpenMU ConnectServer, which speaks Plain (no encryption).
    if (!connection_) {
        connection_.emplace(tcp_, proto::Connection::Codec::Plain);
        connect_session_.emplace();
        log::info("App: ConnectServer session opened against %s:%u",
                  server_host_.c_str(),
                  static_cast<unsigned>(server_port_));
    }

    try {
        auto packets = connection_->poll_packets();
        for (const auto& pkt : packets) {
            log_packet("<<-", pkt);
            connect_session_->on_packet(pkt.data(), pkt.size());
        }
    } catch (const std::exception& e) {
        log::error("App: codec error: %s", e.what());
    }

    // After we know the server list, ask for connection info on the
    // first entry exactly once.
    if (!connect_info_requested_ &&
        connect_session_->phase() ==
            proto::ConnectServerSession::Phase::ServerListReceived &&
        !connect_session_->server_list().empty()) {
        connect_session_->request_connection_info(
            connect_session_->server_list().front().id);
        connect_info_requested_ = true;
    }

    for (const auto& pkt : connect_session_->take_outbound()) {
        log_packet("->>", pkt);
        if (!connection_->send_packet(pkt.data(), pkt.size())) {
            log::error("App: failed to ship outbound packet");
        }
    }
}

void App::pump_game_server() {
    if (tcp_.state() != net::TcpState::Connected) {
        return;
    }
    // Lazy init on first reach of `Connected` after switching stage.
    if (!connection_) {
        connection_.emplace(tcp_, proto::Connection::Codec::GameServer);
        game_session_.emplace();
        log::info("App: GameServer session opened against %s:%u",
                  server_host_.c_str(),
                  static_cast<unsigned>(server_port_));
    }

    try {
        auto packets = connection_->poll_packets();
        for (const auto& pkt : packets) {
            log_packet("<<-", pkt);
            game_session_->on_packet(pkt.data(), pkt.size());
        }
    } catch (const std::exception& e) {
        log::error("App: GameServer codec error: %s", e.what());
    }

    // Outbound queue is empty in M2.6; reserved for login (M3+).
    for (const auto& pkt : game_session_->take_outbound()) {
        log_packet("->>", pkt);
        if (!connection_->send_packet(pkt.data(), pkt.size())) {
            log::error("App: GameServer failed to ship outbound packet");
        }
    }
}

bool App::tick() {
    if (quit_) return false;

    const double now_ms = static_cast<double>(SDL_GetTicks());
    const float  dt     = static_cast<float>((now_ms - last_ms_) / 1000.0);
    last_ms_            = now_ms;

    pump_network();
    scenes_.update(*this, dt);

    renderer_.begin_frame();
    scenes_.render(*this, renderer_);
    renderer_.end_frame();
    return true;
}

}  // namespace mu
