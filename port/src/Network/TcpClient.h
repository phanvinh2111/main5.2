#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace mu::net {

enum class TcpState {
    Idle,
    Resolving,
    Connecting,
    Connected,
    Failed,
    Closed,
};

const char* to_string(TcpState s);

// Non-blocking BSD-socket TCP client. Connect runs on a background thread so
// the SDL3 main callback loop never stalls. recv() drains anything that has
// arrived since the last call; send() enqueues a frame for the worker.
//
// This is the substrate that Milestone 2 will layer the MU packet codec on
// top of. M1 only proves that the socket connects to the server.
class TcpClient {
public:
    TcpClient();
    ~TcpClient();

    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;

    // Kick off an async connect. Safe to call again after disconnect().
    void connect(std::string host, unsigned short port);

    // Snapshot the current state.
    TcpState state() const { return state_.load(std::memory_order_acquire); }

    // Last error message — populated when state() == Failed.
    std::string last_error() const;

    // Queue raw bytes for transmission. Returns false if not connected.
    bool send(const void* data, std::size_t len);

    // Drain whatever the worker has received since the last call.
    std::vector<std::uint8_t> drain_received();

    // Tear the worker down cleanly.
    void disconnect();

    const std::string& host() const { return host_; }
    unsigned short port() const { return port_; }

private:
    void worker_main();

    std::string host_;
    unsigned short port_ = 0;

    std::atomic<TcpState> state_{TcpState::Idle};
    std::atomic<bool>     should_stop_{false};

    int fd_ = -1;

    mutable std::mutex   error_mu_;
    std::string          error_;

    std::mutex             send_mu_;
    std::vector<std::uint8_t> send_buf_;

    std::mutex             recv_mu_;
    std::vector<std::uint8_t> recv_buf_;

    std::thread worker_;
};

}  // namespace mu::net
