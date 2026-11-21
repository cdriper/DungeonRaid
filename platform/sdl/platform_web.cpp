// Web (Emscripten): the assets are preloaded into /assets (platform/web/CMakeLists.txt), the canvas
// fills the page (the game letterboxes itself, as in a resized desktop window), the page's favicon is
// the icon, the browser drives the frames (requestAnimationFrame), and the game never quits: a page
// has nothing to go back to (no Exit button; back on the main menu does nothing).
// Saves: SDL's pref path, /libsdl/DungeonRaid/, is on IndexedDB; platform/web/pre.js mounts and
// loads it before main() runs and stores every change.
#include <emscripten.h>

#include <cstdlib>

#include "platform.h"

namespace plat {

std::string AssetDir() { return "/assets"; }

std::string WindowTitleSuffix(const std::string& /*assetDir*/) { return ""; }   // (always the preloaded assets)

// OPENGL from the start: else SDL_CreateRenderer (WebGL) destroys and recreates the window, and
// undoing fill-document on the destroy throws when the page has gained other elements (browser
// extensions, the desktop app's browser pane), which stops the game before it starts.
SDL_WindowFlags ExtraWindowFlags() { return SDL_WINDOW_FILL_DOCUMENT | SDL_WINDOW_OPENGL; }

void SetWindowIcon(SDL_Window* /*window*/, const std::string& /*assetDir*/) {
    // Nothing to do: the page's favicon (platform/web/shell.html).
}

bool IgnoreBackOnBoard() { return false; }

bool CanQuit() { return false; }

namespace {

// SDL has no browser clipboard (only its own copy in memory). Copy: the async Clipboard API (allowed
// right after the click that asked for it, on https or localhost); where it is missing or refused, a
// dialog shows the code to copy by hand. Paste: reading the clipboard is async and needs a
// permission, but the core wants the text now, so a dialog asks for the code (Ctrl+V / long press).
EM_JS_DEPS(dr_clipboard, "$stringToNewUTF8");

EM_JS(void, dr_copy_text, (const char* text), {
    const s = UTF8ToString(text);
    const byHand = () => window.prompt("Copy your progress code:", s);
    if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(s).catch(byHand);
    } else {
        byHand();
    }
});

EM_JS(char*, dr_ask_text, (), {
    const s = window.prompt("Paste your progress code:", "");
    return stringToNewUTF8(s || "");
});

}  // namespace

bool SetClipboardText(const std::string& text) {
    dr_copy_text(text.c_str());
    return true;   // (or the dialog shows the code)
}

std::string GetClipboardText() {
    char* text = dr_ask_text();
    std::string result = text ? text : "";
    free(text);   // allocated by stringToNewUTF8 (malloc)
    return result;
}

namespace {

struct Loop {
    bool (*step)(void*);
    void (*done)(void*);
    void* arg;
};

void Pass(void* loop) {
    Loop* l = static_cast<Loop*>(loop);
    if (l->step(l->arg)) return;
    emscripten_cancel_main_loop();
    l->done(l->arg);
    delete l;
}

}  // namespace

// The browser calls Pass once per animation frame (fps 0: requestAnimationFrame, which is also the
// vsync SDL_SetRenderVSync asked for). simulate_infinite_loop: main() does not return; its stack is
// dropped, which is why the app lives on the heap.
void RunLoop(bool (*step)(void*), void (*done)(void*), void* arg) {
    emscripten_set_main_loop_arg(&Pass, new Loop{step, done, arg}, 0, true);
}

}  // namespace plat
