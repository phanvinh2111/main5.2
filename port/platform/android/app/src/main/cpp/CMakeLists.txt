# Thin bridge: Android Gradle invokes this file; it just forwards to the
# shared `port/CMakeLists.txt` so iOS, Android, and desktop all build from
# the same source tree.

cmake_minimum_required(VERSION 3.22)
project(mumobile_android LANGUAGES C CXX)

set(MUMOBILE_PLATFORM ANDROID CACHE STRING "")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../.. mumobile)
