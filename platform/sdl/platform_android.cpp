// Android: the assets are copied out of the APK, the window is full screen (immersive), the icon
// comes from the APK, and back is ignored on the board (as the Kotlin shell did: a back gesture from
// the screen edge collides with chains).
#include "platform.h"

namespace plat {

// The APK's assets are not files, but the core and the host read files by path: they are copied to
// <files>/assets/ unless the copy there is from the same build. The list is assets.txt, written by
// the Gradle build: a stamp line (a hash of all files), then one path per line.
std::string AssetDir() {
    const char* internal = SDL_GetAndroidInternalStoragePath();
    if (!internal) return "";
    const std::string dir = std::string(internal) + "/assets";
    size_t size = 0;
    void* data = SDL_LoadFile("assets.txt", &size);   // a relative path: read from the APK
    if (!data) return "";
    const std::string list(static_cast<const char*>(data), size);
    SDL_free(data);
    const std::string listCopy = dir + "/assets.txt";
    if (void* old = SDL_LoadFile(listCopy.c_str(), &size)) {
        const bool same = std::string(static_cast<const char*>(old), size) == list;
        SDL_free(old);
        if (same) return dir;
    }
    size_t start = list.find('\n');   // after the stamp line
    while (start != std::string::npos) {
        start++;
        const size_t end = list.find('\n', start);
        const std::string file = list.substr(start, end == std::string::npos ? std::string::npos : end - start);
        start = end;
        if (file.empty()) continue;
        void* bytes = SDL_LoadFile(file.c_str(), &size);
        if (!bytes) return "";
        const std::string dest = dir + "/" + file;
        SDL_CreateDirectory(dest.substr(0, dest.rfind('/')).c_str());   // with its parents
        const bool ok = SDL_SaveFile(dest.c_str(), bytes, size);
        SDL_free(bytes);
        if (!ok) return "";
    }
    SDL_SaveFile(listCopy.c_str(), list.data(), list.size());   // last: marks the copy complete
    return dir;
}

std::string WindowTitleSuffix(const std::string& /*assetDir*/) { return ""; }   // (no visible title)

SDL_WindowFlags ExtraWindowFlags() { return SDL_WINDOW_FULLSCREEN; }

void SetWindowIcon(SDL_Window* /*window*/, const std::string& /*assetDir*/) {
    // Nothing to do: the launcher icon is in the APK's resources.
}

bool IgnoreBackOnBoard() { return true; }

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
