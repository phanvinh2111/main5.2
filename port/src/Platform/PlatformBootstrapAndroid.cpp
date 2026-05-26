#include "Platform/PlatformBootstrap.h"

namespace mu::platform {

void bootstrap() {
    // Android: no-op for M1. The Java side is expected to call
    // SDL_SetHint("MU.server.host", ...) before launching the native code
    // (see port/platform/android/app/src/main/java/com/munique/mu/MainActivity.java
    // — M2 will wire up the SDL3 Java AAR and add this call). Until then,
    // the default host/port from CMake (180.93.43.39:44405) wins.
}

}  // namespace mu::platform
