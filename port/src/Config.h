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

}  // namespace mu
