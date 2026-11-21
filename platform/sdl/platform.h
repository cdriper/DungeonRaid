// What differs between the platforms the SDL host runs on. main.cpp calls only these; each platform
// has its own platform_<name>.cpp, and CMake compiles the one for the target (platform/sdl/CMakeLists.txt).
#pragma once

#include <SDL3/SDL.h>

#include <string>

namespace plat {

// The folder with textures/, fonts/ and sounds/ (UTF-8, no trailing slash), or "" if there is none.
std::string AssetDir();

// Appended to the window title ("Dungeon Raid <version>"): " (??)" where the assets are not the
// release's own (Windows: no assets/ next to the exe, the repo's are used); else "".
std::string WindowTitleSuffix(const std::string& assetDir);

// Flags added to the window's (resizable, high pixel density).
SDL_WindowFlags ExtraWindowFlags();

// Gives the window its icon where the system does not take it from the program itself.
void SetWindowIcon(SDL_Window* window, const std::string& assetDir);

// host::Config::ignoreBackOnBoard: true where a back gesture from the screen edge can hit the board.
bool IgnoreBackOnBoard();

// host::Config::canQuit: false where the game has nowhere to quit to (a web page): no Exit button,
// and back on the main menu does nothing.
bool CanQuit();

// The system clipboard as UTF-8 text (the progress code's export and import). Set: false if it failed.
bool SetClipboardText(const std::string& text);
std::string GetClipboardText();   // "" when empty or on failure

// The main loop: step(arg) once per pass (it handles the pending events, then draws a frame) until it
// returns false, then done(arg) (shuts down). Where the system drives the frames (the web), RunLoop may
// return before that and both run later: arg must not live on the caller's stack.
void RunLoop(bool (*step)(void*), void (*done)(void*), void* arg);

}  // namespace plat
