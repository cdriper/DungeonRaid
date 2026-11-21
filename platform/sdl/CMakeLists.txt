# The SDL host, the same code on every platform: main.cpp (window, input, audio, the main loop),
# platform_<name>.cpp (what differs: assets, window flags, icon, back; see platform.h) and sdl_gl.cpp
# (the core's drawing calls on SDL_Renderer), on top of the shared host layer (drhost). Linked into DungeonRaid.exe by platform/windows/ and into
# libmain.so by the Android app (platform/android/, its NDK build runs the repo's CMakeLists.txt).
#
# SDL3 is built from third_party/SDL3 (cmake -P third_party/fetch.cmake): statically on the
# desktop, as libSDL3.so on Android (SDLActivity loads it, then libmain.so).

if(ANDROID)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
endif()
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${DR_THIRD_PARTY}/SDL3" "${CMAKE_BINARY_DIR}/third_party/SDL3" EXCLUDE_FROM_ALL)

# What differs between the platforms: platform.h, implemented once per platform.
if(ANDROID)
    set(DR_PLATFORM_SOURCE platform_android.cpp)
elseif(WIN32)
    set(DR_PLATFORM_SOURCE platform_windows.cpp)
elseif(EMSCRIPTEN)
    set(DR_PLATFORM_SOURCE platform_web.cpp)
else()
    message(WARNING "platform/sdl: no platform_<name>.cpp for ${CMAKE_SYSTEM_NAME} yet; the game is not built")
    return()
endif()

# An object library: main() must end up in the exe / libmain.so itself, not in a static library.
add_library(drsdl OBJECT main.cpp sdl_gl.cpp sdl_gl.h platform.h ${DR_PLATFORM_SOURCE})
target_include_directories(drsdl PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
if(ANDROID)
    target_link_libraries(drsdl PUBLIC drhost SDL3::SDL3-shared)
    # The native half of the app; DungeonRaidActivity (an SDLActivity) loads "SDL3" and "main".
    add_library(main SHARED)
    target_link_libraries(main PRIVATE drsdl)
else()
    target_link_libraries(drsdl PUBLIC drhost SDL3::SDL3-static)
    # A development build finds the repo's assets/ when there is no assets/ folder next to the exe
    # (platform_windows.cpp).
    target_compile_definitions(drsdl PRIVATE DR_SOURCE_ASSETS="${CMAKE_SOURCE_DIR}/assets")
endif()
