#pragma once

namespace mu {

// Compile-time default for the MU game-server endpoint. The user requested
// 180.93.43.39:44405; CMake bakes these into the binary, but they can be
// overridden at runtime through the environment (desktop) or SDL hints
// (mobile) — see App.cpp.
#ifndef MUMOBILE_DEFAULT_SERVER_HOST
#define MUMOBILE_DEFAULT_SERVER_HOST "180.93.43.39"
#endif

#ifndef MUMOBILE_DEFAULT_SERVER_PORT
#define MUMOBILE_DEFAULT_SERVER_PORT 44405
#endif

inline constexpr const char* kDefaultServerHost = MUMOBILE_DEFAULT_SERVER_HOST;
inline constexpr unsigned short kDefaultServerPort =
    static_cast<unsigned short>(MUMOBILE_DEFAULT_SERVER_PORT);

inline constexpr const char* kAppTitle  = "MU Mobile (SDL3 port — M1)";
inline constexpr int          kWinWidth  = 1024;
inline constexpr int          kWinHeight = 768;

// LoginLongPasswordRequest defaults.  The account/password placeholders
// here are blanks; a real login is driven by `MU_ACCOUNT` / `MU_PASSWORD`
// env vars on desktop or the matching `MU.account` / `MU.password` SDL
// hints on mobile.  Version & serial are protocol-level constants that
// the original main5.2 client (Source Main 5.2/source/WSclient.cpp:
// lines 108-109) ships verbatim against the OpenMU reference server.
inline constexpr const char* kDefaultAccount        = "";
inline constexpr const char* kDefaultPassword       = "";
inline constexpr const char* kDefaultClientVersion  = "20404";
inline constexpr const char* kDefaultClientSerial   = "k1Pk2jcET48mxL3b";

}  // namespace mu
