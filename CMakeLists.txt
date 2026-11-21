# Dungeon Raid: portable C++17 game core + platform hosts.
#
#   core/              the game (drcore static library)
#   platform/common/   shared host layer (drhost): start-up, settings, saves, PNG/Ogg decoding
#   platform/sdl/      the SDL host (main loop, input, audio, drawing on SDL_Renderer), all platforms
#   platform/windows/  Windows: DungeonRaid.exe (icon, release zip)
#   platform/android/  Gradle project; its NDK build runs this file and builds libmain.so + libSDL3.so
#   platform/web/      the web build (Emscripten): DungeonRaid.html/.js/.wasm/.data (docs/web.md)
#   tests/             golden replay test, host smoke test (desktop builds only)
#
# Desktop build with the tests (Visual Studio 2022); the hosts need the third-party sources first:
#   cmake -P third_party/fetch.cmake
#   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
#   cmake --build build --config Release
#   ctest --test-dir build -C Release --output-on-failure

cmake_minimum_required(VERSION 3.20)
# The port's version: the one line of VERSION (e.g. 0.1, or 0.2rc1 with a suffix), also read by
# the Android Gradle build. project() takes only the leading numbers (0.2rc1 -> 0.2); the full text
# is shown in the build info and names the release zip.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" DR_VERSION LIMIT_COUNT 1)
string(STRIP "${DR_VERSION}" DR_VERSION)
string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" DR_VERSION_NUMBER "${DR_VERSION}")
if(NOT DR_VERSION_NUMBER)
    message(FATAL_ERROR "VERSION must start with a number (e.g. 0.2 or 0.2rc1), not '${DR_VERSION}'")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
project(DungeonRaid VERSION ${DR_VERSION_NUMBER} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Static C runtime everywhere (one .exe without the VC++ redistributable); must match in all targets.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# Everything may end up in a shared library (Android).
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(DR_THIRD_PARTY "${CMAKE_SOURCE_DIR}/third_party")

add_subdirectory(core)

# The hosts need third_party/ (stb, SDL3).
set(DR_HAVE_HOST OFF)
if(EXISTS "${DR_THIRD_PARTY}/stb/stb_image.h" AND EXISTS "${DR_THIRD_PARTY}/stb/stb_vorbis.c")
    set(DR_HAVE_HOST ON)
    add_subdirectory(platform/common)
    if(EXISTS "${DR_THIRD_PARTY}/SDL3/CMakeLists.txt")
        add_subdirectory(platform/sdl)
        if(WIN32)
            add_subdirectory(platform/windows)
        elseif(EMSCRIPTEN)
            add_subdirectory(platform/web)
        endif()
    else()
        message(WARNING "third_party/SDL3 is missing: run `cmake -P third_party/fetch.cmake` to build the game")
    endif()
else()
    message(WARNING "third_party/stb is missing: run `cmake -P third_party/fetch.cmake` to build the game")
endif()

option(DR_BUILD_TESTS "Build the tests" ON)
if(DR_BUILD_TESTS AND NOT ANDROID)
    enable_testing()
    add_subdirectory(tests)
endif()
