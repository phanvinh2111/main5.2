#pragma once

namespace mu::platform {

// Per-platform startup hook called by App::init() *after* SDL_Init() but
// *before* the rest of the app queries SDL hints.
//
// Implementations:
//   * Desktop (`PlatformBootstrapDesktop.cpp`) — no-op.
//   * iOS (`PlatformBootstrapIos.mm`) — copies the MU.server.host /
//     MU.server.port entries from NSBundle.mainBundle.infoDictionary into
//     SDL hints via SDL_SetHint(). SDL_GetHint does not read Info.plist
//     on its own, so this bridge is what makes the documented Info.plist
//     override mechanism work.
//   * Android (`PlatformBootstrapAndroid.cpp`) — currently a no-op. M2
//     will add the Java-side counterpart that calls SDL_SetHint from
//     local.properties or BuildConfig before SDLActivity hands off to
//     native code.
void bootstrap();

}  // namespace mu::platform
