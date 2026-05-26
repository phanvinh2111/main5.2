#pragma once

// Thin SDL_Log wrapper. Mirrors the levels used by the original client's
// `_GlobalFunctions.cpp` debug helpers.

namespace mu::log {

void info(const char* fmt, ...);
void warn(const char* fmt, ...);
void error(const char* fmt, ...);

}  // namespace mu::log
