# `main5.2` → iOS/Android SDL3 Port — Roadmap

Full-port roadmap for replacing the Windows-only `Source Main 5.2/` client
with a portable C++17 + SDL3 implementation that runs natively on iOS and
Android. Each milestone is intended to ship as a stand-alone PR so the work
remains reviewable and bisectable.

Server target: `180.93.43.39:44405` (Season 6.15 OpenMU compatible).

## Milestone summary

| # | Milestone | Owns |
|---|---|---|
| M1 | Skeleton + scene state machine + TCP | Project layout, CMake, SDL3 main callbacks, scene FSM, BSD-socket client. |
| **M2** | **Packet codec (Xor3 / Xor32 / SimpleModulus + framing)** | C++ port of OpenMU's `Network/` codec — byte-exact with upstream C# reference vectors. **(this PR)** |
| M2.5 | Protocol pump (ConnectServer) | Wire `mu::proto::Connection` (Codec::Plain) into the SceneManager, complete the ConnectServer dialogue against `180.93.43.39:44405`, surface the game-server endpoint on the SERVER_LIST scene. |
| **M2.6** | **Game-server dial + GameServerEntered parse** | Tear down ConnectServer codec, re-dial TCP at the resolved endpoint, switch to Codec::GameServer, parse the `F1 00 GameServerEntered` packet, display version/playerId on LOG_IN. **(this PR)** |
| M3 | Renderer foundation | SDL3 GPU wrapper, shader pipeline, 2D sprite/quad batcher, BMFont text. Replaces `glBegin/glEnd` paths in `ZzzOpenglUtil`. |
| M4 | Texture loader (OZJ/OZB/OZT/JPG) | Port the JPEG-Turbo + custom OZJ/OZB decoders from `ZzzTexture.cpp` to SDL3 textures. |
| M5 | Audio | SDL3 audio device + WAV/Ogg streamer replacing `DSplaysound.cpp` + `wzAudio.lib`. |
| M6 | BMD model loader + animation | Port `ZzzBMD.cpp` (skeleton, bone animation, vertex blending) to a renderer-agnostic mesh class. |
| M7 | Terrain & world rendering | Port `zzzLodTerrain.cpp`, `MapManager.cpp`, height map, water shader. |
| M8 | Server List scene parity | Port `ServerList`/login pre-scene UI 1:1 from `LoginWin.cpp`. |
| M9 | Login scene parity | Port `LogIn` scene UI + authentication packet exchange. |
| M10 | Character select scene parity | Port `CharSelMainWin.cpp` + character preview rendering. |
| M11 | Character creation scene | Port `CharMakeWin.cpp`. |
| M12 | Loading scene + world transition | Port `LoadingScene.cpp` and inter-scene map handoff. |
| M13 | Main scene — input + camera | Port `CameraMove.cpp`, mouse→touch input mapping, virtual joystick. |
| M14 | Main scene — HUD & inventory | Port `NewUIMainFrameWindow.cpp`, inventory, vault, chat. |
| M15 | Main scene — combat, skills, parties | Skill bar, target lock, party UI, guild UI. |
| M16 | MU Helper + master skill tree | Port `MUHelper/` + master skill tree from `NewUIMasterLevel.cpp`. |
| M17 | Code signing + store packaging | iOS provisioning profile, Android keystore, CI for IPA/AAB. Requires user credentials. |
| M18 | QA pass + performance tuning | Profile-guided optimization, draw call reduction, asset packing. |

## Detailed scope per milestone

### M1 — Skeleton + scene state machine + TCP *(this PR)*

* Add `port/` directory at repo root with CMake project.
* Vendor SDL3 via `FetchContent` (release tag pinned).
* Implement SDL3 main callbacks (`SDL_AppInit`/`SDL_AppEvent`/`SDL_AppIterate`/
  `SDL_AppQuit`) so the same entry point works on Linux, iOS, and Android.
* Scene state machine that mirrors `enum EGameScene` from
  `Source Main 5.2/source/_define.h`.
* Stub `IScene` for each of the 6 scenes — each renders its own name and the
  current TCP connection state. No 3D.
* Portable BSD-socket TCP client (POSIX `socket`/`connect`/`send`/`recv`)
  with a non-blocking poll loop, hard-coded to dial `180.93.43.39:44405`.
* iOS Xcode CMake target with `Info.plist` and `LaunchScreen.storyboard`.
* Android Gradle/NDK project that links the same CMake target.
* CI workflow that smoke-builds the Linux target.
* Roadmap + README documents.

### M2 — Packet codec

* Ported MUnique/OpenMU's `src/Network` codec to portable C++17 under
  `port/src/Protocol/`:
  * `Framing.{h,cpp}` — C1/C2/C3/C4 prefix dispatch and length read/write.
  * `Keys.h` — the four default key sets (client + server, encrypt +
    decrypt) and the 32-byte rolling XOR key, taken verbatim from
    `MUnique.OpenMU.Network.SimpleModulus.DefaultKeys`.
  * `Xor.{h,cpp}` — Xor3 (3-byte symmetric) and Xor32 (32-byte rolling)
    matching `PipelinedXor32Encryptor` / `Decryptor` byte-for-byte.
  * `SimpleModulus.{h,cpp}` — the "new variant" 8→11-byte block cipher
    with counter and per-block checksum, mirroring
    `PipelinedSimpleModulusEncryptor` / `Decryptor`.
  * `Connection.{h,cpp}` — composes `SimpleModulus(Xor32(plaintext))` on
    send and `Xor32_dec(SimpleModulus_dec(cipher))` on receive, matching
    `Season6Episode3NetworkEncryptionFactoryPlugIn`.
* Comprehensive unit tests (`port/tests/protocol_tests.cpp`) — 15 cases
  including framing, Xor3 known vector, Xor32 round-trip, SimpleModulus
  block size matching upstream's `C3EncryptDecryptCycleAsync` reference,
  counter-mismatch rejection, full pipeline round-trip, and a byte-exact
  decode of OpenMU's `PipelinedDecryptorTests.C3DecryptAsync` cipher.
* Linux CI now runs the test target on every PR (`ctest`).

### M2.5 — Protocol pump (ConnectServer) *(this PR)*

* Add `Codec::Plain` mode to `mu::proto::Connection` so it can speak
  the OpenMU ConnectServer protocol (no Xor32, no SimpleModulus) in
  addition to the Season 6 game-server protocol from M2.
* Add `mu::proto::ConnectServerSession`, an I/O-free state machine
  that drives the ConnectServer dialogue:
  1. Receive `C1 04 00 01` (hello) from the server.
  2. Reply with `C1 04 F4 06` (RequestServerList).
  3. Parse the `F4 06` response into a `vector<ConnectServerEntry>`.
  4. Send `C1 06 F4 03 <id_be>` (RequestConnectionInfo) for the
     first server.
  5. Parse the `F4 03` response into `(game_server_host,
     game_server_port)`.
* Wire the session into `App::tick()` -- `pump_connect_server()`
  shuttles bytes between the `Connection` and the session each frame.
* Display ConnectServer state on the `ServerListScene`: phase, server
  list (id + load%), and the parsed game-server endpoint once we have
  it.  Tap/Enter only advances to `LOG_IN` after we have the endpoint.
* Five new tests in `protocol_tests.cpp` covering the state machine
  (hello -> request, server list parse, connection-info request
  format, connection-info parse, short-packet rejection).
* Verified end-to-end against `180.93.43.39:44405`: the binary
  receives the hello, requests the server list, gets back 1 entry
  (id=0, load=0%), requests the connection info, and displays the
  resolved endpoint `180.93.43.39:55901` on screen.

### M2.6 — Game-server dial + GameServerEntered parse *(this PR)*

* Extend the App's network ownership so it can drive **two** protocol
  stages on the same `TcpClient` -- `NetStage::ConnectServer` (M2.5) and
  `NetStage::GameServer` (this milestone) -- with a clean transition.
* New `App::start_game_server_dial(host, port)`: tears down the
  ConnectServer `Connection` + `ConnectServerSession`, disconnects the
  TCP worker, re-dials at the resolved game-server endpoint, and
  flips `stage_` so the next pump constructs a fresh `Connection` with
  `Codec::GameServer` plus a `GameServerSession`.
* New `mu::proto::GameServerSession` -- I/O-free state machine,
  symmetric to `ConnectServerSession`. The first server packet is a
  plaintext `C1 0C F1 00 ...` "GameServerEntered" frame; the session
  parses its result byte, BE player-id, and 5-byte ASCII version
  string ("10404" on the reference server).
* `ServerListScene` advance now triggers the stage transition rather
  than just changing the scene. `LogInScene` is rewritten to display
  the GameServer phase + result/playerId/version once `Entered`.
* Three new tests in `protocol_tests.cpp` (parses-entered, short-
  payload-marks-error, unknown-packet-not-fatal); 24 total now pass.
* Verified live: against `180.93.43.39`, the binary completes the
  ConnectServer dialogue, transitions to `:55901`, receives the
  `F1 00` packet, and renders
  `Entered: result=0x01 playerId=512 version="10404"` on screen.

### M3 — Renderer foundation

* SDL3 GPU API wrapper (`Render/GpuDevice`, `Render/Pipeline`,
  `Render/Texture`).
* SPIR-V shaders for: textured quad, masked text glyph, alpha-blended sprite.
* Sprite batcher with 16k quad capacity.
* BMFont text renderer using the original `Font0.bmd` glyph atlas.

### M4 — Texture loader

* Decoder for `.OZJ` (XOR'd JPEG), `.OZB` (XOR'd BMP), `.OZT` (XOR'd TGA),
  matching the routines in `ZzzTexture.cpp`.
* Streaming texture cache with LRU eviction on mobile (memory constrained).

### M5 — Audio

* SDL3 audio device + decoded `.WAV` playback via `SDL_LoadWAV_IO`.
* OGG streaming for BGM using `stb_vorbis`.
* Replaces `DSplaysound.cpp` and the `wzAudio.lib` static library.

### M6 — BMD model loader + animation

* Port `ZzzBMD.cpp` to a renderer-agnostic `Mesh` + `Skeleton` class.
* Bone-blended skinning done in shader (no fixed-function transforms).
* Smoke test: load `Player.bmd`, render in `CharacterScene`.

### M7–M12 — Scene parity

Each scene gets its own PR. For every scene, the deliverable is:

1. Exact UI layout & widget tree taken from the corresponding
   `Source Main 5.2/source/*.cpp` file.
2. All input handlers ported.
3. All packet flows that the scene triggers (login, char-select, etc.).
4. Visual diff screenshots vs. the Windows client.

### M13–M16 — In-game systems

Same pattern — one PR per subsystem. M13 is the most touch-input-heavy
since the original client assumes mouse + keyboard.

### M17 — Code signing & store packaging

* iOS: provisioning profile, capabilities, `xcodebuild archive` → IPA.
* Android: release keystore, `bundle release` → AAB ready for Play Console.
* CI: GitHub Actions workflow with macOS runner for iOS, Ubuntu for Android.
* **Blocked on**: Apple Developer credentials, Android signing key from the
  user. Until then, builds are unsigned debug.

### M18 — QA pass

* RenderDoc / Xcode GPU frame capture.
* Reduce draw calls below 200/frame on iPhone 12.
* APK size budget < 300 MB (extract `Data/` to OBB if needed).
* Battery profiling.

## Open questions

* **Protocol fidelity**: server `180.93.43.39:44405` is assumed to be the
  OpenMU Season 6.15 variant described in `README.md`. M2 will validate
  this against a packet capture. If the server uses a non-standard protocol
  (e.g. custom WebZen-derived), M2 spec will need to be revised.
* **Asset distribution**: 737 MB of assets sit under `Source Main 5.2/bin/Data/`.
  iOS IPAs over 200 MB require Wi-Fi to download from the App Store and
  cannot exceed 4 GB. We will likely need an in-app downloader that pulls a
  zipped asset bundle from a CDN on first launch (M4 deliverable).
* **Touch UX**: MU's mouse-driven combat does not map cleanly to touch. M13
  will need a UX pass with the user.
* **Code signing**: M17 requires Apple Developer + Android signing keys from
  the user.

## Tracking

Each milestone PR will reference back to this document and tick the
relevant box.
