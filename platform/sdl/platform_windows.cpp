// Windows: assets next to the exe (a release) or the repo's (a development build), a normal window,
// the icon from the exe's resources, back (Esc) works everywhere as in the original.
#include <filesystem>
#include <system_error>

#include "platform.h"

namespace plat {

namespace {

// The assets/ folder next to the exe (a release), or "" if there is none.
std::string ExeAssetDir() {
    if (const char* base = SDL_GetBasePath()) {
        std::string dir = std::string(base) + "assets";
        std::error_code ec;
        if (std::filesystem::is_directory(std::filesystem::u8path(dir), ec)) return dir;
    }
    return "";
}

}  // namespace

std::string AssetDir() {
    const std::string dir = ExeAssetDir();
    return dir.empty() ? DR_SOURCE_ASSETS : dir;   // set by platform/sdl/CMakeLists.txt: the repo's assets/
}

// "(??)": the assets are not the ones next to the exe (a development build using the repo's).
std::string WindowTitleSuffix(const std::string& assetDir) {
    const std::string exeDir = ExeAssetDir();
    return !exeDir.empty() && assetDir == exeDir ? "" : " (??)";
}

SDL_WindowFlags ExtraWindowFlags() { return 0; }

void SetWindowIcon(SDL_Window* /*window*/, const std::string& /*assetDir*/) {
    // Nothing to do: SDL uses the exe's first icon resource (platform/windows/icon.rc).
}

bool IgnoreBackOnBoard() { return false; }

bool CanQuit() { return true; }

bool SetClipboardText(const std::string& text) { return SDL_SetClipboardText(text.c_str()); }

std::string GetClipboardText() {
    char* text = SDL_GetClipboardText();   // "" (not null) when empty or on failure
    std::string result = text ? text : "";
    SDL_free(text);
    return result;
}

void RunLoop(bool (*step)(void*), void (*done)(void*), void* arg) {
    while (step(arg)) {
    }
    done(arg);
}

}  // namespace plat
