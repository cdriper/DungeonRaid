// The SDL host, the same on every platform (Windows, Android; Linux and macOS should follow): SDL3
// for the window, rendering, input and audio; everything else is the shared host layer
// (platform/common).
//
// Rendering: the core's drawing calls (core/src/dr_gl.h: the original's GLES 1.x calls renamed
// sdl*) are implemented on SDL_Renderer by sdl_gl.cpp: Direct3D 11 on Windows, OpenGL ES 2 on
// Android by default; SDL_RENDER_DRIVER=opengl, vulkan, direct3d12... picks another.
// The window has the canvas's shape, by the Info panel setting: 9:16, 2:3 without the panel, or
// (desktop) the horizontal layout, the board on the left and the HUD and info panel on the right;
// it is refitted when that changes.
// Input: a finger, or the left mouse button, is the touch (one at a time); with the Alt selection
// setting a click on a tile starts a chain, the next click ends it, a right click cancels it. On the
// board Esc opens or closes the pause menu. Esc / Android's back is
// the back key: it quits from the main menu; on Android it is ignored on the board, as the Kotlin
// shell did (a back gesture from the screen edge collides with chains). Alt+Enter or F11 toggles
// fullscreen on the desktop (the Fullscreen setting, saved). While the name screen's keyboard is up, a physical keyboard types into
// it (Backspace, Enter = OK). In debug mode (three taps on the title screen's build info), Ctrl+S
// adds a random skill during a game and Ctrl+A plays an idle animation.
// Files: Windows saves to %APPDATA%\DungeonRaid\, Android to the app's files folder (where the
// Kotlin shell saved).
// What differs between the platforms (assets, window flags, icon, back on the board) is behind
// platform.h, one platform_<name>.cpp per platform; this file has no platform #ifs.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "host.h"
#include "media.h"
#include "platform.h"
#include "sdl_gl.h"
#include "version.h"

namespace {

// ------------------------------------------------------------------------------------------- audio

// A small mixer on one SDL audio stream: every sound is decoded at start-up and converted to the
// stream's format; up to 8 play at once (the Android shell's SoundPool limit), a new one replaces
// the oldest when all are busy, as SoundPool does with equal priorities.
class Mixer {
public:
    static constexpr int kChannels = 2;
    static constexpr int kRate = 44100;
    static constexpr int kVoices = 8;

    bool Open() {
        SDL_AudioSpec spec{SDL_AUDIO_F32, kChannels, kRate};
        m_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &Mixer::Callback, this);
        if (!m_stream) {
            SDL_Log("no audio: %s", SDL_GetError());
            return false;
        }
        SDL_ResumeAudioStreamDevice(m_stream);
        return true;
    }

    void Close() {
        if (m_stream) SDL_DestroyAudioStream(m_stream);
        m_stream = nullptr;
    }

    // Returns the clip id, or -1. Clips are kept even without an audio device (the ids stay valid).
    int Load(const std::string& path) {
        host::Pcm pcm;
        if (!host::DecodeOgg(path, pcm) || pcm.channels <= 0) return -1;
        SDL_AudioSpec src{SDL_AUDIO_S16, pcm.channels, pcm.sampleRate};
        SDL_AudioSpec dst{SDL_AUDIO_F32, kChannels, kRate};
        Uint8* out = nullptr;
        int outLen = 0;
        if (!SDL_ConvertAudioSamples(&src, reinterpret_cast<const Uint8*>(pcm.samples.data()),
                                     static_cast<int>(pcm.samples.size() * sizeof(int16_t)), &dst, &out, &outLen)) {
            return -1;
        }
        const float* f = reinterpret_cast<const float*>(out);
        m_clips.emplace_back(f, f + outLen / sizeof(float));
        SDL_free(out);
        return static_cast<int>(m_clips.size()) - 1;
    }

    void Play(int id) {
        if (!m_stream || id < 0 || id >= static_cast<int>(m_clips.size())) return;
        SDL_LockAudioStream(m_stream);   // the callback runs with this lock held
        Voice* v = &m_voices[0];
        for (Voice& candidate : m_voices) {
            if (candidate.clip < 0) {
                v = &candidate;
                break;
            }
            if (candidate.started < v->started) v = &candidate;
        }
        v->clip = id;
        v->pos = 0;
        v->started = ++m_counter;
        SDL_UnlockAudioStream(m_stream);
    }

    void SetGain(float gain) {
        if (m_stream) SDL_LockAudioStream(m_stream);
        m_gain = gain;
        if (m_stream) SDL_UnlockAudioStream(m_stream);
    }

private:
    struct Voice {
        int clip = -1;
        size_t pos = 0;   // in floats
        uint64_t started = 0;
    };

    static void SDLCALL Callback(void* userdata, SDL_AudioStream* stream, int additional, int /*total*/) {
        static_cast<Mixer*>(userdata)->Mix(stream, additional);
    }

    void Mix(SDL_AudioStream* stream, int bytes) {
        const size_t count = static_cast<size_t>(bytes) / sizeof(float);
        if (count == 0) return;
        m_buffer.assign(count, 0.0f);
        for (Voice& v : m_voices) {
            if (v.clip < 0) continue;
            const std::vector<float>& clip = m_clips[v.clip];
            const size_t n = std::min(count, clip.size() - v.pos);
            for (size_t i = 0; i < n; i++) m_buffer[i] += clip[v.pos + i];
            v.pos += n;
            if (v.pos >= clip.size()) v.clip = -1;
        }
        for (float& s : m_buffer) s = std::clamp(s * m_gain, -1.0f, 1.0f);
        SDL_PutAudioStreamData(stream, m_buffer.data(), static_cast<int>(count * sizeof(float)));
    }

    SDL_AudioStream* m_stream = nullptr;
    std::vector<std::vector<float>> m_clips;
    Voice m_voices[kVoices];
    uint64_t m_counter = 0;
    float m_gain = 1.0f;   // the volume setting
    std::vector<float> m_buffer;
};

// ---------------------------------------------------------------------------------------- platform

class SdlPlatform : public host::Platform {
public:
    explicit SdlPlatform(Mixer& mixer) : m_mixer(mixer) {}

    bool LoadTexture(int glName, const std::string& path) override {
        host::Image image;
        if (!host::LoadImageRgba(path, image, true)) return false;
        return sdlgl::SetTexture(glName, image.width, image.height, image.rgba.data());
    }

    int LoadSound(const std::string& path) override { return m_mixer.Load(path); }
    void PlayEffect(int id) override { m_mixer.Play(id); }
    void SetVolume(float gain) override { m_mixer.SetGain(gain); }

    bool SetClipboardText(const std::string& text) override { return plat::SetClipboardText(text); }
    std::string GetClipboardText() override { return plat::GetClipboardText(); }

private:
    Mixer& m_mixer;
};

void Fail(const char* what) {
    std::string msg = std::string(what) + "\n" + SDL_GetError();
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Dungeon Raid", msg.c_str(), nullptr);
}

// The window's pixel size per window coordinate (mouse events come in window coordinates).
float PixelScale(SDL_Window* window) {
    int w = 0, h = 0, pw = 0, ph = 0;
    SDL_GetWindowSize(window, &w, &h);
    SDL_GetWindowSizeInPixels(window, &pw, &ph);
    return w > 0 ? static_cast<float>(pw) / static_cast<float>(w) : 1.0f;
}

// A finger's position (0..1 across the window) in window pixels.
void FingerPixels(SDL_Window* window, const SDL_TouchFingerEvent& f, float& x, float& y) {
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    x = f.x * static_cast<float>(w);
    y = f.y * static_cast<float>(h);
}

// --------------------------------------------------------------------------------------------- app

// The window, the renderer, the game and the main loop's state. On the heap: plat::RunLoop may return
// before the loop ends (where the system drives the frames), so nothing may live on main()'s stack.
class SdlApp {
public:
    SdlApp() = default;
    ~SdlApp();
    SdlApp(const SdlApp&) = delete;
    SdlApp& operator=(const SdlApp&) = delete;

    // false: could not start (the user was told); ExitCode() is then main()'s result.
    bool Init();
    int ExitCode() const { return m_exitCode; }
    // One pass of the main loop: the pending events, then a frame. false: quit.
    bool Step();

    static bool StepThunk(void* app) { return static_cast<SdlApp*>(app)->Step(); }
    static void DoneThunk(void* app) { delete static_cast<SdlApp*>(app); }

private:
    void Interrupt();
    bool FitWindow(float newW, float newH, float oldW, float oldH);

    bool m_sdl = false;     // SDL_Init succeeded
    bool m_sdlgl = false;   // sdlgl::Init ran
    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    bool m_vsync = false;
    Mixer m_mixer;   // opened after the game's Init: the callback must not run while clips are being added
    SdlPlatform m_platform{m_mixer};
    host::Config m_config;
    // Touch devices have the game's own on-screen keyboard; SDL's text input would open the system one.
    bool m_physicalKeyboard = false;
    std::unique_ptr<host::Host> m_game;
    int m_exitCode = 0;

    bool m_running = true;
    bool m_touching = false;
    SDL_FingerID m_finger = 0;   // the finger that is the touch; 0: the mouse is
    bool m_minimized = false;    // also while in the background (Android)
    // Alt selection (mouse): the release of the click that may have started a chain is
    // decided after the next frame (m_altPendingUp): a one-tile chain keeps the touch going
    // without the button (m_altChain) until the next left click ends it or a right click
    // cancels it; anything else (menus, a drag over more tiles) is an ordinary release.
    bool m_altPendingUp = false;
    bool m_altChain = false;
    // the canvas (screen units) the window was last fitted to: at first the 9:16 one it opens with.
    // A canvas change while fullscreen or maximized is fitted once the window is back to normal.
    float m_windowCanvasW = 1.0f, m_windowCanvasH = 16.0f / 9.0f;
    // the fullscreen state last asked of the window (or reported by it): the Fullscreen
    // setting is applied when it differs (SDL_SetWindowFullscreen completes later, with an event)
    bool m_windowFull = false;
    static constexpr Uint64 kFrameNs = SDL_NS_PER_SECOND / 60;
    Uint64 m_next = 0;   // when the next frame is due (no vsync)
};

SdlApp::~SdlApp() {
    m_game.reset();   // the Host saves its settings here, before the renderer goes
    m_mixer.Close();
    if (m_sdlgl) sdlgl::Shutdown();
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
    if (m_window) SDL_DestroyWindow(m_window);
    if (m_sdl) SDL_Quit();
}

bool SdlApp::Init() {
    SDL_SetAppMetadata("Dungeon Raid", GetVersion(), nullptr);   // the repo's VERSION file
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");   // Android's back is ours: the game quits itself
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "Portrait");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");          // fingers are handled as fingers
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            Fail("Cannot initialise SDL.");
            m_exitCode = 1;
            return false;
        }
    }
    m_sdl = true;

    // Portrait 9:16 (the game's canvas), 85% of the usable desktop height (Android: full screen,
    // from ExtraWindowFlags).
    int winH = 960;
    SDL_Rect usable;
    if (SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable)) {
        winH = static_cast<int>(static_cast<float>(usable.h) * 0.85f);
    }
    const int winW = winH * 9 / 16;
    m_window = SDL_CreateWindow("Dungeon Raid", winW, winH,
                                SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | plat::ExtraWindowFlags());
    if (!m_window) {
        Fail("Cannot create the window.");
        m_exitCode = 1;
        return false;
    }
    SDL_SetWindowMinimumSize(m_window, 240, 360);
    const std::string assetDir = plat::AssetDir();
    plat::SetWindowIcon(m_window, assetDir);
    // the version (the repo's VERSION file); "(??)": not the release's own assets
    const std::string title = std::string("Dungeon Raid ") + GetVersion() + plat::WindowTitleSuffix(assetDir);
    SDL_SetWindowTitle(m_window, title.c_str());
    m_renderer = SDL_CreateRenderer(m_window, nullptr);   // SDL's default driver order
    if (!m_renderer) {
        Fail("Cannot create a renderer.");
        m_exitCode = 1;
        return false;
    }
    SDL_Log("renderer: %s", SDL_GetRendererName(m_renderer));
    m_vsync = SDL_SetRenderVSync(m_renderer, 1);
    sdlgl::Init(m_renderer);
    m_sdlgl = true;

    m_config.assetDir = assetDir;
    char* pref = SDL_GetPrefPath("", "DungeonRaid");
    m_config.saveDir = pref ? pref : ".";
    SDL_free(pref);
    m_config.ignoreBackOnBoard = plat::IgnoreBackOnBoard();
    m_config.canQuit = plat::CanQuit();
    m_config.mouse = SDL_HasMouse();
    m_physicalKeyboard = !SDL_HasScreenKeyboardSupport();
    m_config.desktop = m_physicalKeyboard;   // a desktop: the Horizontal layout is offered, the window fits it

    int pw = 0, ph = 0;
    SDL_GetWindowSizeInPixels(m_window, &pw, &ph);
    m_game = std::make_unique<host::Host>(m_platform, m_config);
    if (!m_game->Init(pw, ph)) {
        Fail(("Cannot load the game's files from " + m_config.assetDir).c_str());
        return false;   // exit code 0, as before
    }
    m_mixer.Open();
    m_next = SDL_GetTicksNS();
    return true;
}

void SdlApp::Interrupt() {
    m_altPendingUp = false;
    m_altChain = false;
    if (!m_touching) return;
    m_touching = false;
    if (m_finger == 0) SDL_CaptureMouse(false);
    m_game->TouchInterrupted();
}

// The window to a new canvas (9:16, or the horizontal layout's; sizes in screen units): the
// window keeps its pixels per unit (as the old canvas fitted in it), so the game keeps its size
// on screen, unless the new window would not fit the desktop (then it shrinks); the window's
// centre kept, pulled back inside the desktop's usable area. false: not now (fullscreen or
// maximized; the window gets its old size back later), so the caller tries again next frame.
bool SdlApp::FitWindow(float newW, float newH, float oldW, float oldH) {
    if (!m_config.desktop) return true;
    const SDL_WindowFlags flags = SDL_GetWindowFlags(m_window);
    if (flags & SDL_WINDOW_FILL_DOCUMENT) return true;   // the web page's canvas follows the browser window
    if (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED)) return false;
    int x = 0, y = 0, w = 0, h = 0;
    SDL_GetWindowPosition(m_window, &x, &y);
    SDL_GetWindowSize(m_window, &w, &h);
    const float perUnit = std::min(static_cast<float>(w) / oldW, static_cast<float>(h) / oldH);
    int nw = static_cast<int>(newW * perUnit + 0.5f), nh = static_cast<int>(newH * perUnit + 0.5f);
    SDL_Rect area;
    const bool haveArea = SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(m_window), &area);
    if (haveArea && (nw > area.w * 95 / 100 || nh > area.h * 95 / 100)) {
        const float fit = std::min(area.w * 0.95f / static_cast<float>(nw), area.h * 0.95f / static_cast<float>(nh));
        nw = static_cast<int>(static_cast<float>(nw) * fit);
        nh = static_cast<int>(static_cast<float>(nh) * fit);
    }
    if (nw == w && nh == h) return true;
    int nx = x + (w - nw) / 2, ny = y + (h - nh) / 2;
    if (haveArea) {
        if (nx + nw > area.x + area.w) nx = area.x + area.w - nw;
        if (ny + nh > area.y + area.h) ny = area.y + area.h - nh;
        if (nx < area.x) nx = area.x;
        if (ny < area.y) ny = area.y;
    }
    SDL_SetWindowSize(m_window, nw, nh);
    SDL_SetWindowPosition(m_window, nx, ny);
    return true;
}

bool SdlApp::Step() {
    host::Host& game = *m_game;
    SDL_Event e;
    // While minimized, sleep until something happens.
    bool have = m_minimized ? SDL_WaitEvent(&e) : SDL_PollEvent(&e);
    while (have) {
        switch (e.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_TERMINATING:
            m_running = false;
            break;
        case SDL_EVENT_WILL_ENTER_BACKGROUND:
            Interrupt();
            m_minimized = true;
            break;
        case SDL_EVENT_DID_ENTER_FOREGROUND:
            m_minimized = false;
            break;
        case SDL_EVENT_TEXT_INPUT:
            game.TypeText(e.text.text);
            break;
        case SDL_EVENT_KEY_DOWN:
            if (e.key.key == SDLK_BACKSPACE) {   // key repeat deletes on, as holding the on-screen key does
                game.TypeBackspace();
                break;
            }
            if (e.key.repeat) break;
            if (e.key.key == SDLK_S && (e.key.mod & SDL_KMOD_CTRL)) {
                game.DebugAddSkill();
            } else if (e.key.key == SDLK_A && (e.key.mod & SDL_KMOD_CTRL)) {
                game.DebugIdleAnimation();
            } else if ((e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER) && !(e.key.mod & SDL_KMOD_ALT)) {
                game.TypeEnter();
            } else if (e.key.key == SDLK_ESCAPE || e.key.key == SDLK_AC_BACK) {
                Interrupt();
                if (!game.Back() && m_config.canQuit) m_running = false;
            } else if (e.key.key == SDLK_F11 ||
                       (e.key.key == SDLK_RETURN && (e.key.mod & SDL_KMOD_ALT))) {
                game.SetFullscreen(!game.Fullscreen());   // applied after the frame
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (e.button.which == SDL_TOUCH_MOUSEID) break;
            if (e.button.button == SDL_BUTTON_LEFT && !m_touching) {
                const float s = PixelScale(m_window);
                m_touching = true;
                m_finger = 0;
                m_altPendingUp = false;
                m_altChain = false;
                SDL_CaptureMouse(true);
                game.TouchStart(e.button.x * s, e.button.y * s, true);
            } else if (e.button.button == SDL_BUTTON_LEFT && m_touching && m_finger == 0 && m_altChain) {
                const float s = PixelScale(m_window);   // the second click ends the chain
                m_altChain = false;
                m_touching = false;
                game.TouchDrag(e.button.x * s, e.button.y * s);
                game.TouchEnd();
            } else if (e.button.button == SDL_BUTTON_RIGHT && m_touching && m_finger == 0 &&
                       (m_altChain || m_altPendingUp || game.AltSelection())) {
                Interrupt();   // a right click cancels the chain
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (e.motion.which == SDL_TOUCH_MOUSEID) break;
            if (m_touching && m_finger == 0) {
                const float s = PixelScale(m_window);
                game.TouchDrag(e.motion.x * s, e.motion.y * s);
            } else if (!m_touching) {   // hover: the info panel describes what is under the mouse
                const float s = PixelScale(m_window);
                game.PointerMove(e.motion.x * s, e.motion.y * s);
            }
            break;
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            game.PointerLeave();
            break;
        case SDL_EVENT_MOUSE_WHEEL: {   // SDL: y > 0 away from the user (scroll up)
            const float y = e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -e.wheel.y : e.wheel.y;
            game.MouseWheel(-y);
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (e.button.button == SDL_BUTTON_LEFT && m_touching && m_finger == 0 && !m_altChain && !m_altPendingUp) {
                SDL_CaptureMouse(false);
                if (game.AltSelection()) {
                    m_altPendingUp = true;   // decided after the next frame
                } else {
                    m_touching = false;
                    game.TouchEnd();
                }
            }
            break;
        case SDL_EVENT_FINGER_DOWN:   // the first finger down is the touch
            if (!m_touching && e.tfinger.fingerID != 0) {
                float x, y;
                FingerPixels(m_window, e.tfinger, x, y);
                m_touching = true;
                m_finger = e.tfinger.fingerID;
                game.TouchStart(x, y);
            }
            break;
        case SDL_EVENT_FINGER_MOTION:
            if (m_touching && m_finger != 0 && e.tfinger.fingerID == m_finger) {
                float x, y;
                FingerPixels(m_window, e.tfinger, x, y);
                game.TouchDrag(x, y);
            }
            break;
        case SDL_EVENT_FINGER_UP:
            if (m_touching && m_finger != 0 && e.tfinger.fingerID == m_finger) {
                m_touching = false;
                game.TouchEnd();
            }
            break;
        case SDL_EVENT_FINGER_CANCELED:
            if (m_finger != 0 && e.tfinger.fingerID == m_finger) Interrupt();
            break;
        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:   // (also the system's own changes)
        case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
            if (!m_config.desktop) break;   // (Android: always full screen)
            m_windowFull = e.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN;
            game.SetFullscreen(m_windowFull);
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
        case SDL_EVENT_WINDOW_HIDDEN:   // (the web: the tab went to the background; frames stop)
            Interrupt();
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            game.Resize(e.window.data1, e.window.data2);
            break;
        case SDL_EVENT_RENDER_DEVICE_RESET:   // the GPU device was recreated: textures are gone
            game.ReloadTextures();
            break;
        case SDL_EVENT_WINDOW_MINIMIZED:
            m_minimized = true;
            break;
        case SDL_EVENT_WINDOW_RESTORED:
        case SDL_EVENT_WINDOW_MAXIMIZED:
            m_minimized = false;
            break;
        default:
            break;
        }
        have = SDL_PollEvent(&e);
    }
    if (!m_running || m_minimized) return m_running;

    // Text input only while the name screen's keyboard is up (it also enables the IME).
    const bool wantText = m_physicalKeyboard && game.WantsText();
    if (wantText != SDL_TextInputActive(m_window)) {
        if (wantText) SDL_StartTextInput(m_window);
        else SDL_StopTextInput(m_window);
    }

    game.Frame();
    if (game.QuitRequested()) {   // the title screen's Exit (desktop)
        m_running = false;
        return false;
    }
    if (m_config.desktop && game.Fullscreen() != m_windowFull) {   // the setting (also as saved, at the start)
        m_windowFull = game.Fullscreen();
        SDL_SetWindowFullscreen(m_window, m_windowFull);
    }
    float canvasW = 0.0f, canvasH = 0.0f;   // the Info panel setting (also as saved, at the start)
    game.CanvasSize(canvasW, canvasH);
    if ((canvasW != m_windowCanvasW || canvasH != m_windowCanvasH) &&
        FitWindow(canvasW, canvasH, m_windowCanvasW, m_windowCanvasH)) {
        m_windowCanvasW = canvasW;
        m_windowCanvasH = canvasH;
    }
    if (m_altPendingUp) {   // the frame has seen the click: did it start a chain?
        m_altPendingUp = false;
        if (m_touching && game.ChainLength() == 1) {
            m_altChain = true;
        } else if (m_touching) {
            m_touching = false;
            game.TouchEnd();
        }
    }
    SDL_RenderPresent(m_renderer);
    if (!m_vsync) {   // no vsync: pace to 60 frames per second
        m_next += kFrameNs;
        const Uint64 now = SDL_GetTicksNS();
        if (m_next > now) SDL_DelayPrecise(m_next - now);
        else m_next = now;
    }
    return true;
}

}  // namespace

int main(int /*argc*/, char** /*argv*/) {
    SdlApp* app = new SdlApp();
    if (!app->Init()) {
        const int code = app->ExitCode();
        delete app;
        return code;
    }
    plat::RunLoop(&SdlApp::StepThunk, &SdlApp::DoneThunk, app);   // deletes the app when the loop ends
    return 0;
}
