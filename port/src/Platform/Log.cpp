#include "Platform/Log.h"

#include <SDL3/SDL_log.h>

#include <cstdarg>
#include <cstdio>

namespace mu::log {

namespace {

void vlog(SDL_LogPriority priority, const char* fmt, va_list args) {
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, priority, fmt, args);
}

}  // namespace

void info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlog(SDL_LOG_PRIORITY_INFO, fmt, args);
    va_end(args);
}

void warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlog(SDL_LOG_PRIORITY_WARN, fmt, args);
    va_end(args);
}

void error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlog(SDL_LOG_PRIORITY_ERROR, fmt, args);
    va_end(args);
}

}  // namespace mu::log
