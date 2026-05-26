#include "Network/TcpClient.h"

#include "Platform/Log.h"

#include <SDL3/SDL_timer.h>

#include <cerrno>
#include <cstring>
#include <utility>

#if defined(_WIN32)
// The mobile port doesn't target Windows; we keep the guard so a developer
// can still drive a desktop build on Windows when iterating.
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "Ws2_32.lib")
using socket_t = SOCKET;
constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#  define MU_CLOSE_SOCKET(s) closesocket(s)
#  define MU_LAST_SOCKET_ERR() (WSAGetLastError())
#else
#  include <arpa/inet.h>
#  include <fcntl.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <poll.h>
#  include <sys/socket.h>
#  include <unistd.h>
using socket_t = int;
constexpr socket_t kInvalidSocket = -1;
#  define MU_CLOSE_SOCKET(s) ::close(s)
#  define MU_LAST_SOCKET_ERR() (errno)
#endif

namespace mu::net {

const char* to_string(TcpState s) {
    switch (s) {
        case TcpState::Idle:       return "Idle";
        case TcpState::Resolving:  return "Resolving";
        case TcpState::Connecting: return "Connecting";
        case TcpState::Connected:  return "Connected";
        case TcpState::Failed:     return "Failed";
        case TcpState::Closed:     return "Closed";
    }
    return "?";
}

TcpClient::TcpClient() = default;

TcpClient::~TcpClient() { disconnect(); }

void TcpClient::connect(std::string host, unsigned short port) {
    disconnect();

    host_ = std::move(host);
    port_ = port;
    state_.store(TcpState::Resolving, std::memory_order_release);
    should_stop_.store(false, std::memory_order_release);

    worker_ = std::thread([this] { worker_main(); });
}

void TcpClient::disconnect() {
    should_stop_.store(true, std::memory_order_release);
    if (worker_.joinable()) {
        worker_.join();
    }
    if (fd_ != kInvalidSocket) {
        MU_CLOSE_SOCKET(fd_);
        fd_ = kInvalidSocket;
    }
    state_.store(TcpState::Closed, std::memory_order_release);
}

std::string TcpClient::last_error() const {
    std::lock_guard<std::mutex> lock(error_mu_);
    return error_;
}

bool TcpClient::send(const void* data, std::size_t len) {
    if (state() != TcpState::Connected || len == 0) return false;
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::lock_guard<std::mutex> lock(send_mu_);
    send_buf_.insert(send_buf_.end(), bytes, bytes + len);
    return true;
}

std::vector<std::uint8_t> TcpClient::drain_received() {
    std::vector<std::uint8_t> out;
    std::lock_guard<std::mutex> lock(recv_mu_);
    out.swap(recv_buf_);
    return out;
}

namespace {

void set_nonblocking(socket_t s) {
#if defined(_WIN32)
    u_long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);
#else
    int flags = fcntl(s, F_GETFL, 0);
    if (flags == -1) flags = 0;
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
}

bool would_block(int err) {
#if defined(_WIN32)
    return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS;
#else
    return err == EAGAIN || err == EWOULDBLOCK || err == EINPROGRESS;
#endif
}

}  // namespace

void TcpClient::worker_main() {
#if defined(_WIN32)
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    // ---- Resolve ----------------------------------------------------------
    addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* res = nullptr;
    char port_str[8];
    std::snprintf(port_str, sizeof(port_str), "%u",
                  static_cast<unsigned>(port_));
    int rc = ::getaddrinfo(host_.c_str(), port_str, &hints, &res);
    if (rc != 0 || res == nullptr) {
        std::lock_guard<std::mutex> lock(error_mu_);
        error_ = std::string("getaddrinfo failed: ") + gai_strerror(rc);
        state_.store(TcpState::Failed, std::memory_order_release);
        log::error("TcpClient: %s", error_.c_str());
#if defined(_WIN32)
        WSACleanup();
#endif
        return;
    }

    state_.store(TcpState::Connecting, std::memory_order_release);

    // ---- Connect ----------------------------------------------------------
    socket_t s = kInvalidSocket;
    for (auto* p = res; p != nullptr; p = p->ai_next) {
        s = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == kInvalidSocket) continue;

        set_nonblocking(s);

        rc = ::connect(s, p->ai_addr, static_cast<int>(p->ai_addrlen));
        if (rc == 0) {
            break;  // immediate success — rare
        }
        if (!would_block(MU_LAST_SOCKET_ERR())) {
            MU_CLOSE_SOCKET(s);
            s = kInvalidSocket;
            continue;
        }

        // Wait up to 5s for writability => connection established.
#if defined(_WIN32)
        fd_set wset; FD_ZERO(&wset); FD_SET(s, &wset);
        timeval tv{5, 0};
        rc = ::select(static_cast<int>(s) + 1, nullptr, &wset, nullptr, &tv);
#else
        pollfd pfd{s, POLLOUT, 0};
        rc = ::poll(&pfd, 1, 5000);
#endif
        if (rc > 0) {
            int err = 0;
            socklen_t err_len = sizeof(err);
            if (::getsockopt(s, SOL_SOCKET, SO_ERROR,
                             reinterpret_cast<char*>(&err), &err_len) == 0 &&
                err == 0) {
                break;
            }
        }
        MU_CLOSE_SOCKET(s);
        s = kInvalidSocket;
    }

    ::freeaddrinfo(res);

    if (s == kInvalidSocket) {
        std::lock_guard<std::mutex> lock(error_mu_);
        error_ = "connect() failed (timeout or refused)";
        state_.store(TcpState::Failed, std::memory_order_release);
        log::error("TcpClient: %s", error_.c_str());
#if defined(_WIN32)
        WSACleanup();
#endif
        return;
    }

    int one = 1;
    ::setsockopt(s, IPPROTO_TCP, TCP_NODELAY,
                 reinterpret_cast<const char*>(&one), sizeof(one));

    fd_ = s;
    state_.store(TcpState::Connected, std::memory_order_release);
    log::info("TcpClient: connected to %s:%u", host_.c_str(),
              static_cast<unsigned>(port_));

    // ---- I/O pump --------------------------------------------------------
    std::uint8_t scratch[4096];
    while (!should_stop_.load(std::memory_order_acquire)) {
#if defined(_WIN32)
        fd_set rset; FD_ZERO(&rset); FD_SET(s, &rset);
        timeval tv{0, 100 * 1000};
        ::select(static_cast<int>(s) + 1, &rset, nullptr, nullptr, &tv);
        bool readable = FD_ISSET(s, &rset);
#else
        pollfd pfd{s, POLLIN, 0};
        ::poll(&pfd, 1, 100);
        bool readable = (pfd.revents & POLLIN) != 0;
#endif
        if (readable) {
#if defined(_WIN32)
            int n = ::recv(s, reinterpret_cast<char*>(scratch),
                           sizeof(scratch), 0);
#else
            ssize_t n = ::recv(s, scratch, sizeof(scratch), 0);
#endif
            if (n > 0) {
                std::lock_guard<std::mutex> lock(recv_mu_);
                recv_buf_.insert(recv_buf_.end(), scratch, scratch + n);
            } else if (n == 0) {
                state_.store(TcpState::Closed, std::memory_order_release);
                log::info("TcpClient: peer closed");
                break;
            } else if (!would_block(MU_LAST_SOCKET_ERR())) {
                std::lock_guard<std::mutex> lock(error_mu_);
                error_ = "recv() failed";
                state_.store(TcpState::Failed, std::memory_order_release);
                break;
            }
        }

        // Drain pending sends.
        std::vector<std::uint8_t> out;
        {
            std::lock_guard<std::mutex> lock(send_mu_);
            out.swap(send_buf_);
        }
        std::size_t off = 0;
        while (off < out.size() &&
               !should_stop_.load(std::memory_order_acquire)) {
#if defined(_WIN32)
            int n = ::send(s,
                           reinterpret_cast<const char*>(out.data() + off),
                           static_cast<int>(out.size() - off), 0);
#else
            ssize_t n = ::send(s, out.data() + off, out.size() - off, 0);
#endif
            if (n > 0) {
                off += static_cast<std::size_t>(n);
            } else if (n < 0 && would_block(MU_LAST_SOCKET_ERR())) {
                SDL_Delay(5);
            } else {
                std::lock_guard<std::mutex> lock(error_mu_);
                error_ = "send() failed";
                state_.store(TcpState::Failed, std::memory_order_release);
                // The inner break only escapes the per-frame send loop;
                // we also need to stop the outer I/O pump so the worker
                // doesn't keep polling a dead socket (and so the original
                // "send() failed" message isn't clobbered by a follow-up
                // "recv() failed" from the next iteration).
                should_stop_.store(true, std::memory_order_release);
                break;
            }
        }
    }

    MU_CLOSE_SOCKET(s);
    fd_ = kInvalidSocket;
#if defined(_WIN32)
    WSACleanup();
#endif
}

}  // namespace mu::net
