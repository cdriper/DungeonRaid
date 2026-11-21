// See host.h. Ported from the Android shell (GameView.kt, Assets.kt); the JNI calls are replaced by
// what the JNI stubs (core/src/dr_jni.cpp) do.
#include "host.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

#include "app.h"
#include "game.h"
#include "sound.h"
#include "util.h"

namespace fs = std::filesystem;

namespace host {
namespace {

// Settings keys (same as the Android shell's SharedPreferences).
const char* const P_SOUND = "sound";
const char* const P_DIFF = "diff";
const char* const P_MODE = "mode";
const char* const P_SCORE_PAGE = "spg";
const char* const P_NAME = "name";
const char* const P_CLOCK = "clock";
const char* const P_VOLUME = "volume";   // port-only: 0..100 (the settings screen's slider)
const char* const P_DEBUG = "debug";     // port-only: debug mode
const char* const P_ALT_SELECTION = "altsel";   // port-only: mouse click-click chains
const char* const P_PANEL = "panel";                // port-only: the Info panel setting (Game::PanelMode)
const char* const P_PANEL_BOTTOM = "panelbottom";   // (before P_PANEL) the info panel below the board
const char* const P_HORIZONTAL = "horizontal";      // (before P_PANEL) the desktop's horizontal layout
const char* const P_FULLSCREEN = "fullscreen";      // port-only: the desktop's fullscreen window
const char* const P_GRIND = "grind";                // port-only: the trophy rate, 0 original, 1 fast, 2 faster
const char* const P_DIM = "dim";
const char* const P_TROPHY = "tro";
const char* const P_CLASS = "ccla";

const char* const SETTINGS_FILE = "settings.ini";
const char* const SAVE_FILE = "save";
const char* const CLASS_FILE = "cla";
const char* const ACHIEVEMENT_FILE = "ach";   // port: the achievements (Game::SaveAchievements)
const char* const LEGACY_SCORE_FILE = "scores2";
const char* const SCORE_FILES[] = {"scea", "scno", "scha", "schr", "scds", "scph"};   // ScoreType order
const int SCORE_FILE_COUNT = 6;

// MAX_*_STATE_SIZE of the original (the GameState capacities).
const int MAX_SAVE_STATE_SIZE = 0x800;
const int MAX_CLASS_STATE_SIZE = 0x400;
const int MAX_SCORE_STATE_SIZE = 0x400;

// GL texture names = Texture enum values (TEX_TILES = 1 ... TEX_FONT_SCORE = 6).
const char* const TEXTURE_FILES[] = {"tiles", "font", "icons", "menus", "font_menu", "font_score"};

fs::path Utf8Path(const std::string& s) { return fs::u8path(s); }

void Fill(GameState& state, const std::vector<unsigned char>& bytes) {
    for (unsigned char b : bytes) state.WriteChar(b);
}

}  // namespace

// ---------------------------------------------------------------------------------------- Settings

Settings::Settings(std::string path) : m_path(std::move(path)) {
    std::ifstream in(Utf8Path(m_path), std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t eq = line.find('=');
        if (eq == std::string::npos || eq == 0) continue;
        m_values[line.substr(0, eq)] = line.substr(eq + 1);
    }
}

int Settings::GetInt(const std::string& key, int fallback) const {
    auto it = m_values.find(key);
    if (it == m_values.end()) return fallback;
    try {
        return std::stoi(it->second);
    } catch (...) {
        return fallback;
    }
}

bool Settings::GetBool(const std::string& key, bool fallback) const {
    auto it = m_values.find(key);
    if (it == m_values.end()) return fallback;
    return it->second == "true";
}

std::string Settings::GetString(const std::string& key, const std::string& fallback) const {
    auto it = m_values.find(key);
    return it == m_values.end() ? fallback : it->second;
}

void Settings::SetInt(const std::string& key, int value) { SetString(key, std::to_string(value)); }
void Settings::SetBool(const std::string& key, bool value) { SetString(key, value ? "true" : "false"); }

void Settings::SetString(const std::string& key, const std::string& value) {
    std::string clean = value;   // one value per line
    for (char& c : clean) {
        if (c == '\n' || c == '\r') c = ' ';
    }
    auto it = m_values.find(key);
    if (it != m_values.end() && it->second == clean) return;
    m_values[key] = clean;
    m_dirty = true;
}

void Settings::Save() {
    if (!m_dirty) return;
    std::ofstream out(Utf8Path(m_path), std::ios::binary | std::ios::trunc);
    for (const auto& kv : m_values) out << kv.first << '=' << kv.second << '\n';
    if (out) m_dirty = false;
}

// -------------------------------------------------------------------------------------------- Host

Host::Host(Platform& platform, Config config)
    : m_platform(platform),
      m_config(std::move(config)),
      m_settings((Utf8Path(m_config.saveDir) / SETTINGS_FILE).u8string()) {}

Host::~Host() {
    if (m_app) {
        m_settings.Save();
        m_app->done();
        delete m_app;
    }
}

bool Host::Init(int viewWidth, int viewHeight) {
    // Sounds first, as the Android splash screen does (the sound table is global).
    InitialiseSoundMappings();
    for (int i = 0; i < SOUND_COUNT; i++) {
        const char* name = GetSoundName(i);
        if (!name || !*name) continue;
        for (int j = 1; j <= GetSoundVariations(i); j++) {
            std::string path = m_config.assetDir + "/sounds/" + name + std::to_string(j) + ".ogg";
            int id = m_platform.LoadSound(path);
            if (id < 0) {
                std::fprintf(stderr, "missing sound %s\n", path.c_str());
                continue;
            }
            AddSoundMapping(i, id);
        }
    }
    if (!ReloadTextures()) return false;

    m_app = new App();
    m_app->init(static_cast<float>(viewWidth), static_cast<float>(viewHeight));
    m_game = m_app->m_game;
    m_game->m_desktop = m_config.desktop;
    m_game->m_canQuit = m_config.canQuit;
    m_game->SetViewSize(static_cast<float>(viewWidth), static_cast<float>(viewHeight));
    // the Info panel setting (the tests' Game keeps None, the original 2:3); before it had its own
    // key: Horizontal (desktop), else Panel at bottom, else Top
    int panel = m_settings.GetInt(P_PANEL, -1);
    if (panel < 0) {
        panel = m_config.desktop && m_settings.GetBool(P_HORIZONTAL, false) ? Game::PANEL_HORIZONTAL
                : m_settings.GetBool(P_PANEL_BOTTOM, false)                 ? Game::PANEL_BOTTOM
                                                                            : Game::PANEL_TOP;
    }
    m_game->SetPanelMode(panel);
    m_game->m_panelModeChanged = false;

    ApplySettings();
    const std::string fonts = m_config.assetDir + "/fonts/";
    m_game->CreateFont((fonts + "font.fnt").c_str(), (fonts + "font_menu.fnt").c_str(),
                       (fonts + "font_score.fnt").c_str());
    LoadSaves();
    const bool sound = m_settings.GetBool(P_SOUND, true);
    m_game->Start(sound);
    m_game->m_soundStateChanged = false;
    // the volume: what was saved (full if never set); sound off shows as 0, sound on as at least 5
    int volume = m_settings.GetInt(P_VOLUME, 100);
    volume = volume < 0 ? 0 : (volume > 100 ? 100 : volume);
    if (sound && volume == 0) volume = 100;
    m_game->m_soundVolume = sound ? volume : 0;
    m_game->m_soundVolumeChanged = false;
    ApplyVolume();
    m_game->m_debugMode = m_settings.GetBool(P_DEBUG, false);
    m_game->m_mouseAvailable = m_config.mouse;
    m_game->m_altSelection = m_settings.GetBool(P_ALT_SELECTION, false);
    m_game->m_grind = ClampI(m_settings.GetInt(P_GRIND, 0), 0, 2);
    m_game->m_fullscreen = m_config.desktop && m_settings.GetBool(P_FULLSCREEN, false);
    return true;
}

bool Host::Fullscreen() const { return m_game && m_game->m_fullscreen; }

void Host::SetFullscreen(bool on) {
    if (m_game && m_config.desktop) m_game->SetFullscreen(on);
}

bool Host::AltSelection() const { return m_game && m_game->m_mouseAvailable && m_game->m_altSelection; }

int Host::ChainLength() const { return m_game ? static_cast<int>(m_game->m_chain.size()) : 0; }

float Host::CanvasAspect() const { return m_game ? m_game->CanvasAspect() : 0.5625f; }

void Host::CanvasSize(float& w, float& h) const {
    if (m_game) {
        m_game->CanvasSize(w, h);
    } else {
        w = 1.0f;
        h = 16.0f / 9.0f;
    }
}

void Host::ApplyVolume() {
    const float v = static_cast<float>(m_game->m_soundVolume) / 100.0f;
    m_platform.SetVolume(v * v);
}

bool Host::ReloadTextures() {
    bool ok = true;
    for (int t = 0; t < 6; t++) {
        std::string path = m_config.assetDir + "/textures/" + TEXTURE_FILES[t] + ".png";
        if (!m_platform.LoadTexture(t + 1, path)) {
            std::fprintf(stderr, "cannot load texture %s\n", path.c_str());
            ok = false;
        }
    }
    return ok;
}

void Host::Resize(int viewWidth, int viewHeight) {
    if (m_game) m_game->SetViewSize(static_cast<float>(viewWidth), static_cast<float>(viewHeight));
}

void Host::ApplySettings() {
    Game* g = m_game;
    const Settings& s = m_settings;
    // as Game_SetDifficulty / SetMode / SetScorePage / SetName (the JNI stubs)
    if (s.Has(P_DIFF)) {
        Difficulty v = static_cast<Difficulty>(ClampI(s.GetInt(P_DIFF, 0), 0, 3));
        g->m_difficultyChanged = g->m_difficulty != v;
        g->m_difficulty = v;
    }
    if (s.Has(P_MODE)) {
        GameMode v = static_cast<GameMode>(ClampI(s.GetInt(P_MODE, 0), 0, 1));
        g->m_modeChanged = g->m_mode != v;
        g->m_mode = v;
    }
    if (s.Has(P_SCORE_PAGE)) {
        int v = static_cast<int>(ClampI(s.GetInt(P_SCORE_PAGE, 0), 0, 5));
        g->m_scorePageChanged = g->m_scorePage != v;
        g->m_scorePage = v;
    }
    if (s.Has(P_NAME)) {
        std::string name = s.GetString(P_NAME, "");
        g->m_nameChanged = std::strcmp(g->m_name, name.c_str()) != 0;
        std::strncpy(g->m_name, name.c_str(), 0x10);
        g->m_name[0x10] = '\0';
    }
    g->SetClockState(s.GetBool(P_CLOCK, false));
    g->SetAttackDimmingState(s.GetBool(P_DIM, true));
    // (the Android shell's "smooth" setting is gone: the SDL host always draws at the display's rate)
    if (s.Has(P_TROPHY)) g->SetTimeToTrophy(s.GetInt(P_TROPHY, 0));
    if (s.Has(P_CLASS)) g->m_currentClass = s.GetInt(P_CLASS, 0);
}

void Host::LoadSaves() {
    Game* g = m_game;
    std::vector<unsigned char> bytes;
    if (ReadFile(SAVE_FILE, bytes) && static_cast<int>(bytes.size()) < MAX_SAVE_STATE_SIZE) Fill(g->m_saveState, bytes);
    if (ReadFile(CLASS_FILE, bytes) && static_cast<int>(bytes.size()) < MAX_CLASS_STATE_SIZE) Fill(g->m_classState, bytes);
    if (ReadFile(ACHIEVEMENT_FILE, bytes) && !g->LoadAchievements(bytes)) {
        std::fprintf(stderr, "damaged %s: the achievements start empty\n", ACHIEVEMENT_FILE);
    }

    // One file per score table; the original passes the file length as the version.
    GameState& st = g->m_scoreScreen.m_state;
    bool newScores = false;
    for (int type = 0; type < SCORE_FILE_COUNT; type++) {
        if (!ReadFile(SCORE_FILES[type], bytes)) continue;
        newScores = true;
        st.ResetStates();
        if (static_cast<int>(bytes.size()) < MAX_SCORE_STATE_SIZE) {
            Fill(st, bytes);
            g->m_scoreScreen.DeserialiseState(static_cast<int>(bytes.size()), type);
        }
    }
    if (!newScores && ReadFile(LEGACY_SCORE_FILE, bytes)) {
        st.ResetStates();
        if (static_cast<int>(bytes.size()) < MAX_SCORE_STATE_SIZE) {
            Fill(st, bytes);
            g->m_scoreScreen.DeserialiseLegacyState();
        }
    }
}

void Host::Frame() {
    if (!m_game) return;
    if (m_game->m_clockEnabled) {
        // Java's "h:mm a": hour without a leading zero, AM/PM
        char buf[32];
        std::time_t now = std::time(nullptr);
        const std::tm* local = std::localtime(&now);   // not thread-safe; the host has one thread
        if (!local) return;
        std::strftime(buf, sizeof buf, "%I:%M %p", local);
        m_game->m_interface.SetClockString(buf[0] == '0' ? buf + 1 : buf);
    }
    m_app->process();
    m_app->render();
    AfterFrame();
}

void Host::AfterFrame() {
    Game* g = m_game;
    g->ProcessAchievementQueue();   // (the Java shell sent them to OpenFeint)
    if (g->m_achievementsChanged) {
        g->m_achievementsChanged = false;
        const std::vector<uint8_t> bytes = g->SaveAchievements();
        WriteFile(ACHIEVEMENT_FILE, bytes.data(), static_cast<int>(bytes.size()));
    }
    for (int sound = g->GetNextSound(); sound >= 0; sound = g->GetNextSound()) {
        int id = GetSoundMapping(sound);
        if (id >= 0) m_platform.PlayEffect(id);
    }

    Settings& s = m_settings;
    if (g->m_soundStateChanged) {
        s.SetBool(P_SOUND, g->m_soundEnabled);
        g->m_soundStateChanged = false;
    }
    if (g->m_debugModeChanged) {
        g->m_debugModeChanged = false;
        s.SetBool(P_DEBUG, g->m_debugMode);
    }
    if (g->m_clipboardWrite) {   // progress export (settings)
        g->m_clipboardWrite = false;
        const bool ok = m_platform.SetClipboardText(g->m_clipboardOut);
        g->m_clipboardOut.clear();
        g->ShowToast(ok ? "Progress copied to the clipboard" : "Cannot use the clipboard");
    }
    if (g->m_clipboardReadRequested) {   // progress import (settings)
        g->m_clipboardReadRequested = false;
        g->m_settingsScreen.ReceiveImport(m_platform.GetClipboardText());
    }
    if (g->m_panelModeChanged) {   // (the platform host fits the window to CanvasSize)
        g->m_panelModeChanged = false;
        s.SetInt(P_PANEL, g->m_panelMode);
    }
    if (g->m_fullscreenChanged) {   // (the platform host applies it to the window)
        g->m_fullscreenChanged = false;
        s.SetBool(P_FULLSCREEN, g->m_fullscreen);
    }
    if (g->m_altSelectionChanged) {
        g->m_altSelectionChanged = false;
        s.SetBool(P_ALT_SELECTION, g->m_altSelection);
    }
    if (g->m_grindChanged) {
        g->m_grindChanged = false;
        s.SetInt(P_GRIND, g->m_grind);
    }
    if (g->m_soundVolumeChanged) {
        g->m_soundVolumeChanged = false;
        if (g->m_soundVolume > 0) s.SetInt(P_VOLUME, g->m_soundVolume);   // 0 is "sound off"; the level stays for later
        ApplyVolume();
    }
    if (g->m_difficultyChanged) {
        s.SetInt(P_DIFF, g->m_difficulty);
        g->m_difficultyChanged = false;
    }
    if (g->m_modeChanged) {
        g->m_modeChanged = false;
        s.SetInt(P_MODE, g->m_mode);
    }
    if (g->m_scorePageChanged) {
        g->m_scorePageChanged = false;
        s.SetInt(P_SCORE_PAGE, g->m_scorePage);
    }
    if (g->m_nameChanged) {
        s.SetString(P_NAME, g->m_name);
        g->m_nameChanged = false;
    }
    if (g->m_clockStateChanged) {
        g->m_clockStateChanged = false;
        s.SetBool(P_CLOCK, g->m_clockEnabled);
    }
    if (g->m_dimmingStateChanged) {
        g->m_dimmingStateChanged = false;
        s.SetBool(P_DIM, g->m_attackDimmingEnabled);
    }
    g->m_highFramerateStateChanged = false;   // (not a setting of the port: not saved)
    if (g->m_currentClassChanged) {
        g->m_currentClassChanged = false;
        s.SetInt(P_CLASS, g->m_classes[g->m_currentClass].m_class);
    }
    while (g->m_scoreScreen.HaveScoresChanged()) {
        g->m_scoreScreen.SerialiseChangedState();
        int type = g->m_scoreScreen.m_serialisedType;
        const GameState& st = g->m_scoreScreen.m_state;
        if (type >= 0 && type < SCORE_FILE_COUNT && st.GetStateSize() != 0) {
            WriteFile(SCORE_FILES[type], st.GetBuffer(), st.GetStateSize());
        }
    }
    if (g->m_saveState.HasStateChanged()) {
        if (g->m_saveState.GetStateSize() != 0) {
            WriteFile(SAVE_FILE, g->m_saveState.GetBuffer(), g->m_saveState.GetStateSize());
        } else {
            RemoveFile(SAVE_FILE);
        }
        if (g->m_timeToTrophyChanged) {
            g->m_timeToTrophyChanged = false;
            s.SetInt(P_TROPHY, g->m_timeToTrophy);
        }
        g->m_saveState.ClearStateChanged();
    }
    if (g->m_classState.HasStateChanged()) {
        if (g->m_classState.GetStateSize() != 0) {
            WriteFile(CLASS_FILE, g->m_classState.GetBuffer(), g->m_classState.GetStateSize());
        }
        g->m_classState.ClearStateChanged();
        if (g->m_timeToTrophyChanged) {   // port: also with the classes (a progress import, outside a game)
            g->m_timeToTrophyChanged = false;
            s.SetInt(P_TROPHY, g->m_timeToTrophy);
        }
    }
    // The original's OpenFeint dashboard, nag screen and score posting: dropped (flags cleared).
    g->m_nagScreenCountChanged = false;
    g->m_showFeint = false;
    g->m_showNagScreen = false;
    g->m_postScore = false;
    s.Save();
}

// ------------------------------------------------------------------------------------------- input

void Host::ToViewport(float x, float y, float& gx, float& gy) const {
    // The viewport is centred, so the GL (bottom-up) and window (top-down) offsets are the same.
    // y in the units Game::TouchStart takes (it multiplies by 1.5): screen widths / 1.5, so the
    // taller 9:16 canvas maps the same way as the original 2:3 one.
    const float w = static_cast<float>(m_game->m_viewportWidth);
    gx = (x - static_cast<float>(m_game->m_viewportX)) / w;
    gy = (y - static_cast<float>(m_game->m_viewportY)) / (w * 1.5f);
}

void Host::PointerMove(float x, float y) {
    if (!m_game || m_game->m_viewportWidth <= 0 || m_game->m_viewportHeight <= 0) return;
    float gx, gy;
    ToViewport(x, y, gx, gy);
    m_game->PointerMove(gx, gy);
}

void Host::PointerLeave() {
    if (m_game) m_game->PointerLeave();
}

void Host::MouseWheel(float notches) {
    if (m_game) m_game->MouseWheel(notches);
}

void Host::TouchStart(float x, float y, bool mouse) {
    if (!m_game || m_game->m_viewportWidth <= 0 || m_game->m_viewportHeight <= 0) return;
    float gx, gy;
    ToViewport(x, y, gx, gy);
    m_game->TouchStart(gx, gy, mouse);
}

void Host::TouchDrag(float x, float y) {
    if (!m_game || m_game->m_viewportWidth <= 0 || m_game->m_viewportHeight <= 0) return;
    float gx, gy;
    ToViewport(x, y, gx, gy);
    m_game->TouchDrag(gx, gy);
}

void Host::TouchEnd() {
    if (m_game) m_game->TouchEnd();
}

void Host::TouchInterrupted() {
    if (m_game) m_game->TouchInterrupted();
}

bool Host::Back() {
    if (!m_game) return true;
    if (m_game->m_settingsScreen.m_active || m_game->m_achievementsScreen.m_active) {   // back closes them, over any screen
        m_game->BackPressed();
        return true;
    }
    if (m_game->m_mainMenu.m_active) return false;
    const bool onBoard = m_game->IsBoardScreen() && !m_game->m_gameOverScreen.m_active;
    if (onBoard) {
        // Android ignores back on the board (gestures); a real Esc opens or closes the pause menu
        if (!m_config.ignoreBackOnBoard) m_game->TogglePauseMenu();
    } else {
        m_game->BackPressed();
    }
    return true;
}

bool Host::QuitRequested() const { return m_game && m_game->m_quitRequested; }

void Host::DebugAddSkill() {
    if (m_game) m_game->DebugAddRandomSkill();
}

void Host::DebugIdleAnimation() {
    if (m_game) m_game->DebugIdleAnimation();
}

// ---------------------------------------------------------------------------------------- keyboard

bool Host::WantsText() const { return m_game && m_game->m_nameScreen.AcceptsTyping(); }

void Host::TypeText(const char* utf8) {
    if (!m_game || !utf8) return;
    for (const char* p = utf8; *p; ++p) {
        // ASCII only (the font and the on-screen keyboard have letters and space); bytes of
        // multi-byte UTF-8 sequences are >= 0x80 and ignored by TypeCharacter.
        m_game->m_nameScreen.TypeCharacter(*p);
    }
}

void Host::TypeBackspace() {
    if (m_game) m_game->m_nameScreen.TypeBackspace();
}

void Host::TypeEnter() {
    if (m_game) m_game->m_nameScreen.TypeEnter();
}

// ------------------------------------------------------------------------------------------- files

bool Host::ReadFile(const std::string& name, std::vector<unsigned char>& data) const {
    std::ifstream in(Utf8Path(m_config.saveDir) / name, std::ios::binary);
    if (!in) return false;
    data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

void Host::WriteFile(const std::string& name, const unsigned char* data, int size) const {
    // Write a temporary file and rename it, so a crash mid-write cannot destroy the old save.
    fs::path target = Utf8Path(m_config.saveDir) / name;
    fs::path temp = target;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(data), size);
        if (!out) {
            std::fprintf(stderr, "cannot write %s\n", name.c_str());
            return;
        }
    }
    std::error_code ec;
    fs::rename(temp, target, ec);
    if (ec) std::fprintf(stderr, "cannot write %s: %s\n", name.c_str(), ec.message().c_str());
}

void Host::RemoveFile(const std::string& name) const {
    std::error_code ec;
    fs::remove(Utf8Path(m_config.saveDir) / name, ec);
}

}  // namespace host
