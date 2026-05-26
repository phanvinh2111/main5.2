// iOS / macOS bridge that copies the server-override entries from
// Info.plist into SDL hints so the rest of the codebase can read them via
// SDL_GetHint(). SDL itself does not consult NSBundle, so without this
// helper the Info.plist keys documented in port/README.md would be inert.

#import <Foundation/Foundation.h>

#include <SDL3/SDL_hints.h>

#include "Platform/PlatformBootstrap.h"

namespace mu::platform {

namespace {

void copy_info_plist_string(NSDictionary* info, NSString* key) {
    id value = info[key];
    if (![value isKindOfClass:[NSString class]]) return;
    const char* k = [key UTF8String];
    const char* v = [(NSString*)value UTF8String];
    if (!k || !v || !*k || !*v) return;
    SDL_SetHint(k, v);
}

}  // namespace

void bootstrap() {
    NSDictionary* info = [[NSBundle mainBundle] infoDictionary];
    if (!info) return;
    copy_info_plist_string(info, @"MU.server.host");
    copy_info_plist_string(info, @"MU.server.port");
}

}  // namespace mu::platform
