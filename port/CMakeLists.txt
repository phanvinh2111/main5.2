cmake_minimum_required(VERSION 3.22)
project(mumobile
    VERSION 0.1.0
    DESCRIPTION "MU Online 5.2 — SDL3 iOS/Android port (Milestone 1)"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(MUMOBILE_PLATFORM "DESKTOP" CACHE STRING "Build target: DESKTOP, IOS, ANDROID")
set_property(CACHE MUMOBILE_PLATFORM PROPERTY STRINGS DESKTOP IOS ANDROID)

option(MUMOBILE_USE_VENDORED_SDL "Fetch SDL3 via FetchContent" ON)

# ---------------------------------------------------------------------------
# SDL3
# ---------------------------------------------------------------------------
if(MUMOBILE_USE_VENDORED_SDL)
    include(FetchContent)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.10
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)
else()
    find_package(SDL3 CONFIG REQUIRED)
endif()

# ---------------------------------------------------------------------------
# mumobile core library
# ---------------------------------------------------------------------------
set(MUMOBILE_CORE_SOURCES
    src/App.cpp
    src/Platform/Log.cpp
    src/Network/TcpClient.cpp
    src/Protocol/Framing.cpp
    src/Protocol/Xor.cpp
    src/Protocol/SimpleModulus.cpp
    src/Protocol/Connection.cpp
    src/Scene/SceneManager.cpp
    src/Scene/ServerListScene.cpp
    src/Scene/WebzenScene.cpp
    src/Scene/LogInScene.cpp
    src/Scene/LoadingScene.cpp
    src/Scene/CharacterScene.cpp
    src/Scene/MainScene.cpp
    src/Render/Renderer.cpp
)

# Per-platform bootstrap (no-op on desktop; bridges Info.plist -> SDL hints
# on iOS; future Android Java->native bridge lives next to it).
if(MUMOBILE_PLATFORM STREQUAL "IOS")
    enable_language(OBJCXX)
    list(APPEND MUMOBILE_CORE_SOURCES src/Platform/PlatformBootstrapIos.mm)
elseif(MUMOBILE_PLATFORM STREQUAL "ANDROID")
    list(APPEND MUMOBILE_CORE_SOURCES src/Platform/PlatformBootstrapAndroid.cpp)
else()
    list(APPEND MUMOBILE_CORE_SOURCES src/Platform/PlatformBootstrapDesktop.cpp)
endif()

add_library(mumobile_core STATIC ${MUMOBILE_CORE_SOURCES})

target_include_directories(mumobile_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(mumobile_core PUBLIC SDL3::SDL3-static)

# Default server, overridable at configure-time:
#   -DMUMOBILE_SERVER_HOST=... -DMUMOBILE_SERVER_PORT=...
set(MUMOBILE_SERVER_HOST "180.93.43.39" CACHE STRING "Default server host")
set(MUMOBILE_SERVER_PORT "44405"       CACHE STRING "Default server port")
target_compile_definitions(mumobile_core PUBLIC
    MUMOBILE_DEFAULT_SERVER_HOST="${MUMOBILE_SERVER_HOST}"
    MUMOBILE_DEFAULT_SERVER_PORT=${MUMOBILE_SERVER_PORT})

if(MUMOBILE_PLATFORM STREQUAL "ANDROID")
    target_compile_definitions(mumobile_core PUBLIC MUMOBILE_ANDROID=1)
    # log + android system libs are provided by the NDK.
    target_link_libraries(mumobile_core PUBLIC log android)
elseif(MUMOBILE_PLATFORM STREQUAL "IOS")
    target_compile_definitions(mumobile_core PUBLIC MUMOBILE_IOS=1)
else()
    target_compile_definitions(mumobile_core PUBLIC MUMOBILE_DESKTOP=1)
endif()

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
if(MUMOBILE_PLATFORM STREQUAL "ANDROID")
    # The Android Gradle build builds a shared library named "main" that the
    # SDLActivity Java side dlopens.
    add_library(main SHARED src/Platform/AndroidMain.cpp)
    target_link_libraries(main PRIVATE mumobile_core)
elseif(MUMOBILE_PLATFORM STREQUAL "IOS")
    set(MUMOBILE_IOS_LAUNCH_STORYBOARD
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/ios/LaunchScreen.storyboard)

    add_executable(mumobile MACOSX_BUNDLE
        src/Platform/DesktopMain.cpp
        ${MUMOBILE_IOS_LAUNCH_STORYBOARD})
    target_link_libraries(mumobile PRIVATE mumobile_core)

    # Bundle resources: the storyboard has to be compiled by ibtool and
    # copied into the .app bundle root so UIKit can find it.
    set_source_files_properties(${MUMOBILE_IOS_LAUNCH_STORYBOARD}
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    set_target_properties(mumobile PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/platform/ios/Info.plist
        MACOSX_BUNDLE_GUI_IDENTIFIER com.munique.mu
        MACOSX_BUNDLE_BUNDLE_NAME "MU Mobile"
        MACOSX_BUNDLE_BUNDLE_VERSION "0.1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        # M1 ships an unsigned simulator build only; M17 owns code signing.
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
else()
    add_executable(mumobile src/Platform/DesktopMain.cpp)
    target_link_libraries(mumobile PRIVATE mumobile_core)
endif()

message(STATUS "mumobile platform: ${MUMOBILE_PLATFORM}")
message(STATUS "mumobile default server: ${MUMOBILE_SERVER_HOST}:${MUMOBILE_SERVER_PORT}")

# ---------------------------------------------------------------------------
# Tests (desktop only — CI smoke build runs these)
# ---------------------------------------------------------------------------
if(MUMOBILE_PLATFORM STREQUAL "DESKTOP")
    enable_testing()
    add_executable(mumobile_protocol_tests tests/protocol_tests.cpp)
    target_link_libraries(mumobile_protocol_tests PRIVATE mumobile_core)
    add_test(NAME protocol_tests COMMAND mumobile_protocol_tests)
endif()
