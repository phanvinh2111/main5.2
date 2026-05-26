#include "Platform/PlatformBootstrap.h"

namespace mu::platform {

void bootstrap() {
    // Desktop: nothing to do. MU_SERVER_HOST / MU_SERVER_PORT env vars
    // are read directly in App.cpp.
}

}  // namespace mu::platform
