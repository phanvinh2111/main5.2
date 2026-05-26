# MU `main5.2` — SDL3 Mobile Port

This directory contains the iOS/Android port of the MU Online Season 5.2 client,
rewritten on top of [SDL3](https://github.com/libsdl-org/SDL).

The original client (`Source Main 5.2/`) is a Windows-only application built
against WGL/OpenGL fixed-function, DirectSound, WinAPI windowing, and a
C# .NET 9 Native AOT networking DLL. The mobile port replaces every Windows
dependency with portable C++17 + SDL3:

| Original (Windows) | Mobile port |
|---|---|
| `WinMain` / `HWND` / message loop | `SDL_AppInit`/`SDL_AppEvent`/`SDL_AppIterate` (SDL3 main callbacks) |
| WGL / OpenGL 1.x fixed function | SDL3 GPU API (Metal on iOS, Vulkan on Android, GL on desktop) |
| DirectSound + `wzAudio.lib` | SDL3 audio (planned, milestone M5) |
| Win32 sockets + .NET AOT DLL | BSD sockets + handwritten C++ packet codec |
| Registry / DPAPI / IMM | SDL3 hints + plain text config |
| Visual Studio `.vcxproj` | CMake + Xcode/Gradle generators |

## Status

This branch ships **Milestone 2 (M2)** of the multi-PR full port roadmap on
top of M1. See [`ROADMAP.md`](./ROADMAP.md) for the full plan.

What M1 delivered:

* CMake build system targeting Linux (host/dev), iOS, and Android via SDL3
* SDL3 main-callbacks entry point shared across all platforms
* Scene state machine matching the original `EGameScene` enum (6 scenes:
  `SERVER_LIST`, `WEBZEN`, `LOG_IN`, `LOADING`, `CHARACTER`, `MAIN`)
* Stub `IScene` implementations for each scene (renders the scene name and
  current connection state — no 3D yet)
* Cross-platform TCP client that connects to the configured server
  (defaults to `180.93.43.39:44405`)
* iOS Xcode-compatible CMake target with `Info.plist`
* Android Gradle/NDK project that links against the same CMake target
* Default game server address baked in as a compile-time define so
  `EGameScene::SERVER_LIST` can dial straight to `180.93.43.39:44405`

What M2 adds (this PR):

* `port/src/Protocol/` — a portable C++17 port of the MUnique/OpenMU
  network codec used by the Season 6 Episode 3 client:
  * `Framing` — C1/C2/C3/C4 prefix dispatch + length field read/write.
  * `Keys` — four default key sets (client + server, encrypt + decrypt)
    plus the 32-byte rolling XOR key from
    `MUnique.OpenMU.Network.SimpleModulus.DefaultKeys`.
  * `Xor` — Xor3 (3-byte symmetric) and Xor32 (32-byte rolling).
  * `SimpleModulus` — 8→11-byte block cipher with counter + checksum.
  * `Connection` — composes the full client outbound chain
    `SimpleModulus_enc(Xor32_enc(plaintext))` over `TcpClient`, and the
    matching inbound chain on receive.
* `port/tests/protocol_tests.cpp` — 15 unit tests including a
  byte-exact decode of OpenMU's own `PipelinedDecryptorTests.C3DecryptAsync`
  reference vector.
* Linux CI now runs the test target on every PR (`ctest`).

What this branch explicitly does **not** deliver (see roadmap for which
milestone owns each item):

* 3D rendering, BMD model loading, OZJ/OZB textures, terrain (M3–M6)
* Audio (M5)
* Wiring the codec into the SceneManager / replacing the stub hello (M2.5)
* Character creation, inventory, skills, NPC, party, guild, chat UI (M7–M15)
* MU Helper bot, master skill tree (M16+)
* Code-signed IPA / Play Store AAB (M17, requires user-supplied credentials)

## Building

### Linux desktop (smoke test / CI)

```bash
cd port
cmake -S . -B build -G Ninja -DMUMOBILE_PLATFORM=DESKTOP
cmake --build build
./build/mumobile
```

This downloads SDL3 via `FetchContent` and produces a desktop executable
`build/mumobile` that opens a window, runs the scene state machine, and
attempts a TCP connection to the configured server. Useful for iterating on
non-platform-specific code without needing Xcode/Android Studio.

### iOS (requires macOS + Xcode 15+)

```bash
cd port
cmake -S . -B build-ios -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DMUMOBILE_PLATFORM=IOS
cmake --build build-ios --config Debug \
  -- -sdk iphonesimulator -arch arm64
```

Open `build-ios/mumobile.xcodeproj` in Xcode to run on a device or the
simulator. Code signing is NOT configured (M17 deliverable).

### Android (requires Android Studio Arctic Fox+ with NDK r26+)

```bash
cd port/platform/android
./gradlew assembleDebug
```

The Gradle build invokes CMake under the hood via `externalNativeBuild`
and produces `app/build/outputs/apk/debug/app-debug.apk`.

## Layout

```
port/
├── CMakeLists.txt              # Cross-platform build entry
├── ROADMAP.md                  # Full multi-milestone roadmap
├── README.md                   # You are here
├── include/
│   └── mu/                     # Public headers
├── src/
│   ├── App.{h,cpp}             # SDL3 app shell (init, event, iterate, quit)
│   ├── Config.h                # Compile-time defaults (server IP/port)
│   ├── Network/
│   │   └── TcpClient.{h,cpp}   # Portable BSD-socket TCP client
│   ├── Render/
│   │   └── Renderer.{h,cpp}    # SDL3 GPU wrapper (M1: 2D quad/text only)
│   ├── Scene/
│   │   ├── IScene.h            # Scene interface
│   │   ├── SceneManager.{h,cpp}
│   │   ├── ServerListScene.{h,cpp}
│   │   ├── WebzenScene.{h,cpp}
│   │   ├── LogInScene.{h,cpp}
│   │   ├── LoadingScene.{h,cpp}
│   │   ├── CharacterScene.{h,cpp}
│   │   └── MainScene.{h,cpp}
│   └── Platform/
│       ├── Log.{h,cpp}              # Thin SDL_Log wrapper
│       ├── PlatformBootstrap.h     # Per-platform startup hook
│       ├── PlatformBootstrapDesktop.cpp  # desktop: no-op
│       ├── PlatformBootstrapAndroid.cpp  # android: no-op (Java side TBD)
│       └── PlatformBootstrapIos.mm      # iOS: Info.plist -> SDL_SetHint
└── platform/
    ├── ios/
    │   ├── Info.plist
    │   └── LaunchScreen.storyboard
    └── android/
        ├── settings.gradle
        ├── build.gradle
        └── app/
            ├── build.gradle
            └── src/main/
                ├── AndroidManifest.xml
                ├── cpp/CMakeLists.txt    # Thin wrapper around port/CMakeLists.txt
                └── java/com/munique/mu/MainActivity.java
```

## Server target

The mobile client connects to:

```
180.93.43.39:44405
```

The endpoint is overridable at runtime through two distinct channels:

* **Desktop**: set the `MU_SERVER_HOST` / `MU_SERVER_PORT` environment
  variables before launching `mumobile`.
* **iOS**: edit the `MU.server.host` / `MU.server.port` keys in
  `port/platform/ios/Info.plist`. `PlatformBootstrapIos.mm` runs at
  startup, reads these from `NSBundle.mainBundle.infoDictionary`, and
  publishes them via `SDL_SetHint()`. (SDL itself does NOT read
  `Info.plist`, so this bridge is what makes the override work.)
* **Android**: M2 will add a Java-side `SDL_SetHint` call that reads
  the values from `BuildConfig` (populated from `local.properties`).
  Until then, the CMake-baked default wins on Android.

`App::init()` first checks the env var (desktop) then the SDL hint
(`MU.server.host` / `MU.server.port`), then falls back to the CMake
compile-time default. Resolution lives in `port/src/App.cpp`.
