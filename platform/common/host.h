// The platform-independent part of a host: what the Android shell (GameView.kt, Assets.kt) does
// around the core, in C++, so every platform shares it. A platform host creates the window and GL
// context, implements Platform (textures, sounds), and calls Host from its main loop.
//
// Same behaviour as the Android shell: start-up order, settings keys, save file names and formats
// (saves can be copied between devices), the per-frame polling of sounds, settings and save blobs.
#pragma once

#include <map>
#include <string>
#include <vector>

class App;
class Game;

namespace host {

// What only the platform can do. Called from the thread that runs Host.
class Platform {
public:
    virtual ~Platform() = default;
    // Uploads the image at `path` as GL texture name `glName` (the core binds names 1..6 directly).
    virtual bool LoadTexture(int glName, const std::string& path) = 0;
    // Loads a sound file; returns an id for PlayEffect, or -1.
    virtual int LoadSound(const std::string& path) = 0;
    virtual void PlayEffect(int id) = 0;
    // The sound effects' gain, 0..1 (amplitude); the host already shaped the settings slider's curve.
    virtual void SetVolume(float gain) = 0;
    // The system clipboard as UTF-8 text (progress export/import). Without one: false / "".
    virtual bool SetClipboardText(const std::string& /*text*/) { return false; }
    virtual std::string GetClipboardText() { return std::string(); }
};

// Key/value settings with the Android shell's keys (SharedPreferences "DungeonRaidPrefs"), stored as
// `key=value` lines in one file.
class Settings {
public:
    explicit Settings(std::string path);
    bool Has(const std::string& key) const { return m_values.count(key) != 0; }
    int GetInt(const std::string& key, int fallback) const;
    bool GetBool(const std::string& key, bool fallback) const;
    std::string GetString(const std::string& key, const std::string& fallback) const;
    void SetInt(const std::string& key, int value);
    void SetBool(const std::string& key, bool value);
    void SetString(const std::string& key, const std::string& value);
    void Save();   // writes the file if anything changed

private:
    std::string m_path;
    std::map<std::string, std::string> m_values;
    bool m_dirty = false;
};

struct Config {
    std::string assetDir;   // contains textures/, fonts/, sounds/ (UTF-8, ends without a slash)
    std::string saveDir;    // writable; saves and settings go here (UTF-8)
    // The Android shell ignores back while the board is showing (a back gesture from the screen
    // edge collides with chains); with a real Esc key the original behaviour (main menu) is better.
    bool ignoreBackOnBoard = false;
    // A mouse is present: the settings offer Alt selection (click-click chains).
    bool mouse = false;
    // A desktop (a resizable window): the Info panel setting offers the Horizontal layout.
    bool desktop = false;
    // The game can quit (false on a web page): with desktop, the title screen shows Exit.
    bool canQuit = true;
};

class Host {
public:
    Host(Platform& platform, Config config);
    ~Host();
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;

    // Loads textures and sounds, creates the game for a view of this size (pixels), applies the
    // settings, loads the saves and starts the game. False if an asset is missing.
    bool Init(int viewWidth, int viewHeight);
    // The view (window) changed size: the game's viewport is recomputed.
    void Resize(int viewWidth, int viewHeight);
    // Re-uploads the textures (after the GL context was recreated).
    bool ReloadTextures();
    // One frame: clock, Process, Render, then the polling (sounds, settings, saves).
    void Frame();

    // Input in view pixels (origin top left).
    // mouse: the touch is the mouse button (the chain arrow follows the pointer, selected tiles pop)
    void TouchStart(float x, float y, bool mouse = false);
    void TouchDrag(float x, float y);
    void TouchEnd();
    void TouchInterrupted();
    // The mouse over the view without a button down (the info panel's hover info), and leaving it.
    void PointerMove(float x, float y);
    void PointerLeave();
    // The mouse wheel: notches, > 0 scrolls down (the achievements list).
    void MouseWheel(float notches);
    // Alt selection (a setting): the mouse starts a chain with a click and ends it with the next
    // one; the platform host does the clicks. ChainLength: the tiles in the chain being made.
    bool AltSelection() const;
    // The canvas's width / height (changes with the Info panel setting): desktop hosts fit the window.
    float CanvasAspect() const;
    // The canvas in screen units (the board is 1 wide): a window keeps its pixels per unit when it
    // changes, so the game keeps its size on screen.
    void CanvasSize(float& w, float& h) const;
    int ChainLength() const;
    // The Fullscreen setting (desktop, off at first): the platform host keeps the window in that
    // state, and reports its own changes (Alt+Enter, F11, the system's) with SetFullscreen, which saves it.
    bool Fullscreen() const;
    void SetFullscreen(bool on);
    // The back key. Returns false when the app should quit (back on the main menu).
    bool Back();
    // The title screen's Exit was chosen (desktop only): the host should quit as on back from the
    // main menu.
    bool QuitRequested() const;
    // Debug mode (three taps on the title screen's build info): a random new skill during a game
    // (Ctrl+S on the desktop). Does nothing outside debug mode.
    void DebugAddSkill();
    // Debug mode: one idle animation now (Ctrl+A on the desktop).
    void DebugIdleAnimation();

    // A physical keyboard on the name screen (while its on-screen keyboard is up). WantsText says
    // when the host should deliver text input (e.g. SDL_StartTextInput); TypeText takes UTF-8 and
    // passes letters and spaces on.
    bool WantsText() const;
    void TypeText(const char* utf8);
    void TypeBackspace();
    void TypeEnter();

    Game* GetGame() const { return m_game; }

private:
    void ToViewport(float x, float y, float& gx, float& gy) const;
    void ApplySettings();
    void ApplyVolume();   // the Game's volume (0..100) to the platform, squared: the slider's middle sounds half as loud
    void LoadSaves();
    void AfterFrame();
    bool ReadFile(const std::string& name, std::vector<unsigned char>& data) const;
    void WriteFile(const std::string& name, const unsigned char* data, int size) const;
    void RemoveFile(const std::string& name) const;

    Platform& m_platform;
    Config m_config;
    Settings m_settings;
    App* m_app = nullptr;
    Game* m_game = nullptr;
};

}  // namespace host
