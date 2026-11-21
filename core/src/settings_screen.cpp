#include "settings_screen.h"

#include <cstdio>

#include "dr_gl.h"
#include "font.h"
#include "character_class.h"
#include "game.h"
#include "mode_screen.h"
#include "util.h"

namespace {

// The settings. A toggle: a Game flag and its setter; a slider: a Game value (0..100) and its
// setter. The setters also tell the host to save (and apply) the value. `visibleIf`: a Game flag
// the row is shown for (nullptr: always), checked when the screen opens.
// An action: `action` (kExport, kImport, kReset, the debug ones) for RunAction.
// A choice: a Game value (0..choiceCount-1) and its setter; a tap steps to the next of `choices`.
enum class Kind { Toggle, Slider, Action, Choice };
struct Setting {
    const char* name;
    Kind kind;
    bool Game::*flag;
    void (Game::*setFlag)(bool);
    int Game::*value;
    void (Game::*setValue)(int);
    bool Game::*visibleIf;
    int action = 0;
    bool Game::*hiddenIf = nullptr;   // a Game flag the row is hidden for
    const char* const* choices = nullptr;   // a choice: the values' names
    int choiceCount = 0;
    bool Game::*lastChoiceIf = nullptr;   // a choice: the last value is offered only while this Game flag is set
};
const char* const kGrindNames[] = {"Original", "Fast", "Faster"};
const char* const kPanelNames[] = {"None", "Top", "Bottom", "Horizontal"};   // Game::PanelMode
constexpr int kExport = 1;
constexpr int kImport = 2;
// the Debug page (debug mode, from the pause menu)
constexpr int kDebugSkill = 3;
constexpr int kDebugMonster = 4;
constexpr int kDebugLevelUp = 5;
constexpr int kDebugShop = 6;
constexpr int kDebugUpgrade = 7;
constexpr int kDebugAchievement = 8;
constexpr int kReset = 9;
constexpr int kDebugUnlockAll = 10;
const Setting kSettings[] = {
    {"Volume", Kind::Slider, nullptr, nullptr, &Game::m_soundVolume, &Game::SetSoundVolume, nullptr},
    {"Help pages", Kind::Toggle, &Game::m_helpEnabled, &Game::SetHelpState, nullptr, nullptr, nullptr},
    {"Clock", Kind::Toggle, &Game::m_clockEnabled, &Game::SetClockState, nullptr, nullptr, nullptr},
    {"Attack dimming", Kind::Toggle, &Game::m_attackDimmingEnabled, &Game::SetAttackDimmingState, nullptr, nullptr, nullptr},
    // the trophy rate (Game::KillSpecialEnemyTile)
    {"Grind", Kind::Choice, nullptr, nullptr, &Game::m_grind, &Game::SetGrind, nullptr, 0, nullptr, kGrindNames, 3},
    // (no "Smooth framerate": the original's Java shell chose the frame rate from it; the SDL host
    // always draws at the display's rate, so Game::m_highFramerate is kept but not offered)
    // only with a mouse: click to start a chain, click to end it, right click to cancel
    {"Alt selection", Kind::Toggle, &Game::m_altSelection, &Game::SetAltSelection, nullptr, nullptr, &Game::m_mouseAvailable},
    // the info panel: none (the original screen), above or below the board, or (desktop only) the
    // horizontal layout: the board on the left, the HUD and the info panel on the right
    {"Info panel", Kind::Choice, nullptr, nullptr, &Game::m_panelMode, &Game::SetPanelMode, nullptr, 0, nullptr, kPanelNames,
     4, &Game::m_desktop},
    // desktop only: the window fills the screen (also Alt+Enter / F11)
    {"Fullscreen", Kind::Toggle, &Game::m_fullscreen, &Game::SetFullscreen, nullptr, nullptr, &Game::m_desktop},
    // the classes' progress through the clipboard (progress_code.h)
    {"Export progress", Kind::Action, nullptr, nullptr, nullptr, nullptr, nullptr, kExport},
    {"Import progress", Kind::Action, nullptr, nullptr, nullptr, nullptr, nullptr, kImport},
    // everything back to a first start, after a Yes / No dialog (title screen only)
    {"Reset progress", Kind::Action, nullptr, nullptr, nullptr, nullptr, nullptr, kReset},
    // only while debug mode is on; unticking it turns debug mode off (the row goes at the next opening)
    {"Debug mode", Kind::Toggle, &Game::m_debugMode, &Game::SetDebugMode, nullptr, nullptr, &Game::m_debugMode},
};
constexpr int kSettingCount = static_cast<int>(sizeof(kSettings) / sizeof(kSettings[0]));

// Page 1: debug actions. Add skill / special monster act at once (the page stays); the dialogs
// close the page and the pause menu, then open (Game::m_debugPendingMenu).
const Setting kDebug[] = {
    {"Add skill", Kind::Action, nullptr, nullptr, nullptr, nullptr, nullptr, kDebugSkill},
    {"Add special monster", Kind::Action, nullptr, nullptr, nullptr, nullptr, nullptr, kDebugMonster},
    {"Level up", Kind::Action, nullptr, nullptr, nullptr, nullptr, nullptr, kDebugLevelUp},
    {"Shop", Kind::Action, nullptr, nullptr, nullptr, nullptr, nullptr, kDebugShop},
    {"Armour upgrade", Kind::Action, nullptr, nullptr, nullptr, nullptr, nullptr, kDebugUpgrade},
    {"Add achievement", Kind::Action, nullptr, nullptr, nullptr, nullptr, nullptr, kDebugAchievement},
    {"Unlock all", Kind::Action, nullptr, nullptr, nullptr, nullptr, nullptr, kDebugUnlockAll},
};
constexpr int kDebugCount = static_cast<int>(sizeof(kDebug) / sizeof(kDebug[0]));

const Setting* TableFor(int page) { return page == 1 ? kDebug : kSettings; }
int TableSizeFor(int page) { return page == 1 ? kDebugCount : kSettingCount; }

constexpr int kBack = -2;                  // m_pressed: the Back button
constexpr float kFade = 0.3f;              // seconds, as ModeScreen
constexpr float kEdge = 0.015625f;         // screen units (the screen is 1 x 1.5)
constexpr float kMargin = 0.009375f;
constexpr float kHeading = 0.075f;         // the panel's title bar and its line
constexpr float kRowHeight = 0.1f;
constexpr float kRowGap = 0.025f;
constexpr float kRowInset = 0.03125f;      // rows inside the panel, left and right
constexpr float kTickSize = 0.075f;
constexpr float kTextScale = 0.12f;
// slider: the track from 40% of the row to the value text; a knob and a reach around the track
constexpr float kTrackStart = 0.4f;
constexpr float kValueWidth = 0.14f;
constexpr float kTrackHeight = 0.02f;
constexpr float kKnobWidth = 0.045f;
constexpr float kKnobHeight = 0.07f;
constexpr float kTrackReach = 0.04f;       // a press this close to the track's ends still grabs it
constexpr int kSliderStep = 5;
const GLcolour4 kRowColour{0x96, 0x96, 0xff, 0xff};    // ModeScreen's panel colour
const GLcolour4 kTrackColour{30, 30, 55, 0xff};
const GLcolour4 kFillColour{0x96, 0x96, 0xff, 0xff};
const GLcolour4 kKnobColour{0xd2, 0xd2, 0xff, 0xff};
const float kPx = 1.0f / 1024.0f;                      // texture 1 pixels

GLcolour4 WithAlpha(GLcolour4 c, uint8_t a) {
    c.a = static_cast<uint8_t>(c.a * a / 0xff);
    return c;
}

}  // namespace

SettingsScreen::SettingsScreen(Game* game) : m_game(game), m_heading(&game->m_fonts[1]) {
    m_background.SetSize(GLfloat2{1.0f, 1.5f});
    m_background.m_position = GLfloat2{0.5f, 0.75f};
    m_background.SetColour(GLcolour4{0, 0, 0, 0xff});
    // the Back button bottom right, the panel's background above it (as ModeScreen)
    m_backButton.AddQuad(GLfloat2{104.0f, 1.0f}, GLfloat2{193.0f, 27.0f}, 2, -1.0f);
    const GLfloat2 size = m_backButton.GetSize();
    m_backButton.SetPosition(GLfloat2{(1.0f - size.x) - kMargin, (1.5f - size.y) - kMargin});
    const float h = m_backButton.m_position.y - kMargin;
    m_panelBackground.SetSize(GLfloat2{1.0f, h});
    m_panelBackground.m_position = GLfloat2{0.5f, h * 0.5f};
    m_panelBackground.SetColour(GLcolour4{45, 45, 75, 0xff});
}

std::vector<int> SettingsScreen::VisibleSettings() const {
    std::vector<int> v;
    for (int i = 0; i < TableSizeFor(m_page); ++i) {
        const Setting& s = TableFor(m_page)[i];
        if (s.visibleIf && !(m_game->*s.visibleIf)) continue;
        if (s.hiddenIf && m_game->*s.hiddenIf) continue;
        v.push_back(i);
    }
    return v;
}

void SettingsScreen::Layout(const std::vector<int>& visible) {
    m_layout = visible;
    const int count = static_cast<int>(visible.size());
    // the rows' pitch shrinks when they would reach the Back button (many rows: desktop, debug mode)
    float rowHeight = kRowHeight, rowGap = kRowGap;
    const float room = m_backButton.m_position.y - kMargin - kEdge;
    const float rows = static_cast<float>(count) * (kRowHeight + kRowGap) + kRowGap;
    if (kHeading + rows > room) {
        const float k = (room - kHeading) / rows;
        rowHeight *= k;
        rowGap *= k;
    }
    const float panelHeight = kHeading + static_cast<float>(count) * (rowHeight + rowGap) + rowGap;
    m_panel = QuadStrip();
    ModeScreen::MakePanel(m_panel, true, 0.96875f, panelHeight, kEdge, kEdge);
    m_game->m_nameScreen.m_modeScreen.MakeHeadingLabel(m_panel, m_heading, m_page == 1 ? "Debug" : "Settings");

    const float x = m_panel.m_position.x + kRowInset;
    const float w = 0.96875f - 2.0f * kRowInset;
    m_rows.clear();
    m_rows.resize(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        Row& r = m_rows[static_cast<size_t>(i)];
        r.setting = visible[static_cast<size_t>(i)];
        const Setting& s = TableFor(m_page)[r.setting];
        const float top = m_panel.m_position.y + kHeading + rowGap + static_cast<float>(i) * (rowHeight + rowGap);
        const float middle = top + rowHeight * 0.5f;
        // the label's y is the baseline; capitals reach about 0.45 of the scale above it
        const float baseline = middle + kTextScale * 0.22f;
        ModeScreen::MakeButton(r.button, w, rowHeight, x, top);
        r.label.m_font = &m_game->m_fonts[0];
        r.label.m_spacing = 2.0f;
        r.label.m_scale = GLfloat2{kTextScale, kTextScale};
        r.label.SetText(s.name, -1);
        r.label.m_position = GLfloat2{x + kRowInset, baseline};
        if (s.kind == Kind::Toggle) {
            r.tick.SetSize(GLfloat2{kTickSize, kTickSize});
            r.tick.m_position = GLfloat2{x + w - kRowInset - kTickSize * 0.5f, middle};
        } else if (s.kind == Kind::Slider) {
            r.trackX = x + w * kTrackStart;
            r.trackW = (x + w - kRowInset - kValueWidth) - r.trackX;
            r.track.SetSize(GLfloat2{r.trackW, kTrackHeight});
            r.track.m_position = GLfloat2{r.trackX + r.trackW * 0.5f, middle};
            r.track.SetColour(kTrackColour);
            r.fill.SetColour(kFillColour);
            ModeScreen::MakeButton(r.knob, kKnobWidth, kKnobHeight, r.trackX, middle - kKnobHeight * 0.5f);
            r.value = r.label;
            r.value.m_position = GLfloat2{x + w - kRowInset - kValueWidth + 0.02f, baseline};
            r.shown = -1;
        } else if (s.kind == Kind::Choice) {
            r.value = r.label;   // right-aligned to valueRight (ShowChoice)
            r.valueRight = x + w - kRowInset;
            r.shown = -1;
        }
    }
}

void SettingsScreen::Show(int page) {
    if (page != m_page) {
        m_page = page;
        m_layout.clear();   // another table: lay out again
    }
    const std::vector<int> visible = VisibleSettings();
    if (visible != m_layout) Layout(visible);
    m_active = true;
    m_closing = false;
    m_fadeTime = kFade;
    m_lastTouchState = TOUCHING;   // the touch that opened the screen is not a press here
    m_pressed = -1;
    UpdateRows();
    SetAlpha(0);
}

void SettingsScreen::FadeOut() {
    if (!m_active || m_closing) return;
    m_dialog = false;
    m_closing = true;
    m_fadeTime = kFade;
}

void SettingsScreen::Back() {
    if (m_dialog) {
        m_dialog = false;
        m_dialogPressed = -1;
        return;
    }
    FadeOut();
}

// The knob at `value`, the fill up to it, the value text.
void SettingsScreen::ShowSliderValue(int row, int value) {
    Row& r = m_rows[static_cast<size_t>(row)];
    if (r.shown == value) return;
    r.shown = value;
    const float at = r.trackX + r.trackW * static_cast<float>(value) / 100.0f;
    r.fill.SetSize(GLfloat2{at - r.trackX, kTrackHeight});
    r.fill.m_position = GLfloat2{(r.trackX + at) * 0.5f, r.track.m_position.y};
    r.knob.SetPosition(GLfloat2{at - kKnobWidth * 0.5f, r.track.m_position.y - kKnobHeight * 0.5f});
    char text[8];
    std::snprintf(text, sizeof text, "%d%%", value);
    r.value.SetText(text, -1);
    SetAlpha(m_alpha);   // the new glyphs and quads get the current alpha
}

// A choice row's value name, right-aligned.
void SettingsScreen::ShowChoice(int row, int value) {
    Row& r = m_rows[static_cast<size_t>(row)];
    if (r.shown == value) return;
    r.shown = value;
    const Setting& s = TableFor(m_page)[SettingIndex(row)];
    r.value.SetText(s.choices[ClampI(value, 0, s.choiceCount - 1)], -1);
    r.value.m_position.x = r.valueRight - r.value.m_width * r.value.m_scale.x;
    SetAlpha(m_alpha);   // the new glyphs get the current alpha
}

int SettingsScreen::SliderValueAt(int row, float x) const {
    const Row& r = m_rows[static_cast<size_t>(row)];
    float f = (x - r.trackX) / r.trackW;
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    const int steps = static_cast<int>(f * (100.0f / kSliderStep) + 0.5f);
    return steps * kSliderStep;
}

void SettingsScreen::UpdateRows() {
    for (int i = 0; i < static_cast<int>(m_rows.size()); ++i) {
        const Setting& s = TableFor(m_page)[SettingIndex(i)];
        if (s.kind == Kind::Toggle) {
            const float v = (m_game->*s.flag) ? 394.0f : 411.0f;   // GameMenu's tick box: ticked / empty
            m_rows[static_cast<size_t>(i)].tick.SetUV(GLfloat2{405.0f * kPx, v * kPx},
                                                      GLfloat2{421.0f * kPx, (v + 16.0f) * kPx});
        } else if (s.kind == Kind::Slider && i != m_pressed) {
            ShowSliderValue(i, m_game->*s.value);
        } else if (s.kind == Kind::Choice) {
            ShowChoice(i, m_game->*s.value);
        }
    }
}

void SettingsScreen::SetAlpha(uint8_t a) {
    m_alpha = a;
    m_background.SetAlpha(a);
    m_panelBackground.SetAlpha(a);
    m_backButton.SetAlpha(m_pressed == kBack ? static_cast<uint8_t>(a * 0x96 / 0xff) : a);
    m_panel.SetAlpha(a);
    m_heading.m_colour.a = a;
    m_heading.UpdateColour();
    for (int i = 0; i < static_cast<int>(m_rows.size()); ++i) {
        Row& r = m_rows[static_cast<size_t>(i)];
        const Kind kind = TableFor(m_page)[SettingIndex(i)].kind;
        // a pressed toggle or action row darkens; a slider row stays (its knob shows the drag)
        GLcolour4 c = (i == m_pressed && kind != Kind::Slider) ? DivideColour(kRowColour, 2.0f) : kRowColour;
        r.button.SetColour(WithAlpha(c, a));
        r.label.m_colour.a = a;
        r.label.UpdateColour();
        if (kind == Kind::Toggle) {
            r.tick.SetAlpha(a);
        } else if (kind == Kind::Slider) {
            r.track.SetColour(WithAlpha(kTrackColour, a));
            r.fill.SetColour(WithAlpha(kFillColour, a));
            r.knob.SetColour(WithAlpha(kKnobColour, a));
            r.value.m_colour.a = a;
            r.value.UpdateColour();
        } else if (kind == Kind::Choice) {
            r.value.m_colour.a = a;
            r.value.UpdateColour();
        }
    }
}

int SettingsScreen::RowAt(const GLfloat2& p) {
    for (int i = 0; i < static_cast<int>(m_rows.size()); ++i) {
        if (m_rows[static_cast<size_t>(i)].button.ContainsPoint(p)) return i;
    }
    return -1;
}

// Fades in/out; then a press on a toggle row or the Back button highlights it (with a click) and the
// release on the same one toggles the setting or closes the screen; a press on a slider's track
// moves its knob, which follows the drag, and the release sets the value.
void SettingsScreen::Process(float dt) {
    if (m_fadeTime > 0.0f) {
        float t = m_fadeTime - dt;
        m_fadeTime = t;
        if (t <= 0.0f) {
            m_fadeTime = 0.0f;
            t = 0.0f;
            if (m_closing) {
                m_active = false;
                return;
            }
        }
        const float k = t / kFade;
        SetAlpha(static_cast<uint8_t>(FloatToIntArm(m_closing ? Interpolate(255.0f, 0.0f, k) : Interpolate(0.0f, 255.0f, k))));
        return;
    }
    Game* g = m_game;
    if (m_dialog) {
        ProcessDialog();
        return;
    }
    UpdateRows();   // the values can also change elsewhere
    const GLfloat2 p{g->m_touchX, g->m_touchY};
    const bool sliderPressed = m_pressed >= 0 && TableFor(m_page)[SettingIndex(m_pressed)].kind == Kind::Slider;
    if (g->m_touchState == TOUCHING && m_lastTouchState == NOT_TOUCHING) {
        m_lastTouchState = TOUCHING;
        if (m_backButton.ContainsPoint(p)) {
            m_pressed = kBack;
        } else {
            m_pressed = RowAt(p);
        }
        if (m_pressed >= 0 && TableFor(m_page)[SettingIndex(m_pressed)].kind == Kind::Slider) {
            const Row& r = m_rows[static_cast<size_t>(m_pressed)];
            if (p.x < r.trackX - kTrackReach || p.x > r.trackX + r.trackW + kTrackReach) {
                m_pressed = -1;   // the slider row's name: nothing to do
                return;
            }
            ShowSliderValue(m_pressed, SliderValueAt(m_pressed, p.x));
            return;
        }
        if (m_pressed != -1) {
            g->QueueSound(SOUND_CLICK);
            SetAlpha(m_alpha);   // shows the press
        }
        return;
    }
    if (sliderPressed && g->m_touchState == TOUCHING) {   // dragging the knob
        ShowSliderValue(m_pressed, SliderValueAt(m_pressed, p.x));
        return;
    }
    if (m_lastTouchState != TOUCHING || g->m_touchState != NOT_TOUCHING) return;
    m_lastTouchState = NOT_TOUCHING;
    const int pressed = m_pressed;
    m_pressed = -1;
    SetAlpha(m_alpha);
    if (pressed == kBack) {
        if (m_backButton.ContainsPoint(p)) FadeOut();
    } else if (pressed >= 0 && TableFor(m_page)[SettingIndex(pressed)].kind == Kind::Slider) {
        const Setting& s = TableFor(m_page)[SettingIndex(pressed)];
        (g->*s.setValue)(m_rows[static_cast<size_t>(pressed)].shown);
        g->QueueSound(SOUND_CLICK);   // heard at the new level (nothing when 0: the sound is off)
    } else if (pressed >= 0 && RowAt(p) == pressed) {
        const Setting& s = TableFor(m_page)[SettingIndex(pressed)];
        if (s.kind == Kind::Action) {
            RunAction(s.action);
        } else if (s.kind == Kind::Choice) {
            const int count = s.choiceCount - (s.lastChoiceIf && !(g->*s.lastChoiceIf) ? 1 : 0);
            (g->*s.setValue)(((g->*s.value) + 1) % count);
        } else {
            (g->*s.setFlag)(!(g->*s.flag));
            const std::vector<int> visible = VisibleSettings();   // a toggle can show or hide rows
            if (visible != m_layout) {
                Layout(visible);
                SetAlpha(m_alpha);
            }
        }
    }
    UpdateRows();
}

void SettingsScreen::Render() {
    sdlDisable(GL_TEXTURE_2D);
    m_background.Render();
    m_panelBackground.Render();
    sdlEnable(GL_TEXTURE_2D);
    sdlBindTexture(GL_TEXTURE_2D, 4);
    m_backButton.Render();
    m_panel.Render();
    for (Row& r : m_rows) r.button.Render();
    sdlDisable(GL_TEXTURE_2D);
    for (int i = 0; i < static_cast<int>(m_rows.size()); ++i) {
        if (TableFor(m_page)[SettingIndex(i)].kind != Kind::Slider) continue;
        m_rows[static_cast<size_t>(i)].track.Render();
        m_rows[static_cast<size_t>(i)].fill.Render();
    }
    sdlEnable(GL_TEXTURE_2D);
    for (int i = 0; i < static_cast<int>(m_rows.size()); ++i) {
        if (TableFor(m_page)[SettingIndex(i)].kind == Kind::Slider) m_rows[static_cast<size_t>(i)].knob.Render();
    }
    sdlBindTexture(GL_TEXTURE_2D, 1);
    for (int i = 0; i < static_cast<int>(m_rows.size()); ++i) {
        if (TableFor(m_page)[SettingIndex(i)].kind == Kind::Toggle) m_rows[static_cast<size_t>(i)].tick.Render();
    }
    m_heading.Render();
    for (int i = 0; i < static_cast<int>(m_rows.size()); ++i) {
        Row& r = m_rows[static_cast<size_t>(i)];
        r.label.Render();
        const Kind kind = TableFor(m_page)[SettingIndex(i)].kind;
        if (kind == Kind::Slider || kind == Kind::Choice) r.value.Render();
    }
    if (m_dialog) RenderDialog();
}

// ---------------------------------------------------------------- progress export / import

void SettingsScreen::RunAction(int action) {
    Game* g = m_game;
    if (action == kExport) {
        g->m_clipboardOut = progress::Export(*g);
        g->m_clipboardWrite = true;   // the host copies it and shows the toast
    } else if (action == kImport) {
        // during a game the current class's build is in use: only from the title screen
        if (!g->m_mainMenu.m_active) {
            g->ShowToast("Import progress from the title screen");
            return;
        }
        g->m_clipboardReadRequested = true;   // the host calls ReceiveImport
    } else if (action == kReset) {
        if (!g->m_mainMenu.m_active) {   // during a game its class is in use (as Import)
            g->ShowToast("Reset progress from the title screen");
            return;
        }
        OpenResetDialog();
    } else if (action == kDebugSkill) {
        g->DebugAddRandomSkill();
    } else if (action == kDebugMonster) {
        g->DebugSpawnSpecialEnemy();
    } else if (action == kDebugUnlockAll) {
        g->DebugUnlockAll();   // the page stays
    } else if (action == kDebugAchievement) {
        g->DebugAddAchievement();   // the page stays (its toast shows over it)
    } else if (action == kDebugLevelUp || action == kDebugShop || action == kDebugUpgrade) {
        if (!g->DebugCanAct()) {
            g->ShowToast("Debug: not now (only on the board, between turns)");
            return;
        }
        g->m_debugPendingMenu = action == kDebugLevelUp ? 0 : (action == kDebugShop ? 1 : 2);
        FadeOut();
        GameMenu& menu = g->m_interface.m_gameMenu;   // close the pause menu too (as its Help does)
        if (menu.m_active && !menu.m_closing) {
            menu.m_fadeTime = 0.2f;
            menu.m_closing = true;
        }
    }
}

void SettingsScreen::ReceiveImport(const std::string& text) {
    if (!m_active || m_closing) return;
    progress::Data data;
    const progress::Error e = progress::Parse(text, data);
    if (e != progress::Error::None) {
        m_game->ShowToast(progress::ErrorText(e));
        return;
    }
    m_importData = data;
    OpenDialog();
}

namespace {
constexpr float kDialogTop = 0.1f;
constexpr float kDialogLine = 0.062f;
constexpr float kDialogText = 0.09f;
constexpr float kDialogInset = 0.05f;
constexpr float kDialogButtonW = 0.4f;
constexpr float kDialogButtonH = 0.1f;
const GLcolour4 kWhiteText{0xff, 0xff, 0xff, 0xff};
const GLcolour4 kBetterText{0x78, 0xff, 0x78, 0xff};
const GLcolour4 kWorseText{0xff, 0x78, 0x78, 0xff};

void LevelText(char* out, size_t size, int level) {
    if (level == 0) std::snprintf(out, size, "-");
    else std::snprintf(out, size, "%d", level);
}

int Compare(long long now, long long then) { return then > now ? 1 : (then < now ? -1 : 0); }
}  // namespace

Label& SettingsScreen::AddDialogLine(const char* name, const char* value, int change) {
    const float x = m_dialogPanel.m_position.x;
    const float w = m_dialogPanel.GetSize().x;
    const int line = static_cast<int>(m_dialogLines.size()) / 2;
    float top = m_dialogPanel.m_position.y + kHeading + 0.04f + static_cast<float>(line) * kDialogLine;
    if (line >= 9) top += 0.03f;   // the totals, below the classes
    const float baseline = top + kDialogLine * 0.5f + kDialogText * 0.22f;
    for (int k = 0; k < 2; ++k) {
        m_dialogLines.push_back(Label());
        Label& l = m_dialogLines.back();
        l.m_font = &m_game->m_fonts[0];
        l.m_spacing = 2.0f;
        l.m_scale = GLfloat2{kDialogText, kDialogText};
        l.SetText(k == 0 ? name : value, -1);
        l.m_colour = k == 0 || change == 0 ? kWhiteText : (change > 0 ? kBetterText : kWorseText);
        l.UpdateColour();
        l.m_position.y = baseline;
        l.m_position.x = k == 0 ? x + kDialogInset : x + w - kDialogInset - l.m_width * l.m_scale.x;
    }
    return m_dialogLines.back();
}

// The dialogs' panel (titled `heading`, `height` tall) and their two buttons at its bottom.
void SettingsScreen::MakeDialog(const char* heading, float height, const char* yes, const char* no) {
    Game* g = m_game;
    m_dialogShade.SetSize(GLfloat2{1.0f, 1.5f});
    m_dialogShade.m_position = GLfloat2{0.5f, 0.75f};
    m_dialogShade.SetColour(GLcolour4{0, 0, 0, 190});
    const float w = 0.96875f;
    m_dialogPanel = QuadStrip();
    ModeScreen::MakePanel(m_dialogPanel, true, w, height, kEdge, kDialogTop);
    g->m_nameScreen.m_modeScreen.MakeHeadingLabel(m_dialogPanel, m_dialogHeading, heading);
    m_dialogLines.clear();
    const float top = kDialogTop + height - 0.04f - kDialogButtonH;
    const char* names[2] = {yes, no};
    for (int k = 0; k < 2; ++k) {
        const float bx = k == 0 ? kEdge + 0.0625f : kEdge + w - 0.0625f - kDialogButtonW;
        m_dialogButtons[k] = QuadStrip();
        ModeScreen::MakeButton(m_dialogButtons[k], kDialogButtonW, kDialogButtonH, bx, top);
        Label& l = m_dialogButtonLabels[k];
        l.m_font = &g->m_fonts[0];
        l.m_spacing = 2.0f;
        l.m_scale = GLfloat2{kTextScale, kTextScale};
        l.SetText(names[k], -1);
        l.m_colour = kWhiteText;
        l.UpdateColour();
        l.m_position = GLfloat2{bx + (kDialogButtonW - l.m_width * l.m_scale.x) * 0.5f,
                                top + kDialogButtonH * 0.5f + kTextScale * 0.22f};
    }
    m_dialog = true;
    SetDialogPressed(-1);
}

// The reset dialog: what goes, what stays, Yes / No.
void SettingsScreen::OpenResetDialog() {
    const char* lines[] = {"Delete all progress?", "Classes, scores, achievements", "and the saved game will be lost.",
                           "Settings are kept."};
    const int n = static_cast<int>(sizeof lines / sizeof lines[0]);
    MakeDialog("Reset progress", kHeading + 0.04f + static_cast<float>(n) * kDialogLine + 0.05f + kDialogButtonH + 0.04f,
               "Yes", "No");
    m_dialogKind = kDialogReset;
    for (const char* text : lines) {
        AddDialogLine(text, "", 0);
    }
}

// The dialog: a panel titled "Import progress?" with a line per class (level now -> imported,
// "-" = locked), the totals (classes unlocked, games, kills, the best score, achievements unlocked),
// Import and Cancel.
void SettingsScreen::OpenDialog() {
    Game* g = m_game;
    const progress::Data now = progress::Current(*g);
    const progress::Data& in = m_importData;

    MakeDialog("Import progress?", kHeading + 0.04f + 14.0f * kDialogLine + 0.03f + 0.05f + kDialogButtonH + 0.04f,
               "Import", "Cancel");
    m_dialogKind = kDialogImport;
    char value[48];
    char a[16], b[16];
    int unlockedNow = 0, unlockedIn = 0;
    long long gamesNow = 0, gamesIn = 0, killsNow = 0, killsIn = 0;
    unsigned bestNow = 0, bestIn = 0;
    for (int c = 0; c < 10; ++c) {
        const progress::ClassInfo& n = now.classes[c];
        const progress::ClassInfo& i = in.classes[c];
        gamesNow += n.gamesPlayed;
        gamesIn += i.gamesPlayed;
        killsNow += n.enemiesKilled;
        killsIn += i.enemiesKilled;
        for (int d = 0; d < 3; ++d) {
            if (n.bestScore[d] > bestNow) bestNow = n.bestScore[d];
            if (i.bestScore[d] > bestIn) bestIn = i.bestScore[d];
        }
        if (g->m_classes[c].m_class == CLASS_ADVENTURER) continue;   // always level 1
        unlockedNow += n.level > 0;
        unlockedIn += i.level > 0;
        LevelText(a, sizeof a, n.level);
        LevelText(b, sizeof b, i.level);
        std::snprintf(value, sizeof value, "%s -> %s", a, b);
        AddDialogLine(CharacterClass::GetClassName(g->m_classes[c].m_class), value, Compare(n.level, i.level));
    }
    std::snprintf(value, sizeof value, "%d -> %d", unlockedNow, unlockedIn);
    AddDialogLine("Classes unlocked", value, Compare(unlockedNow, unlockedIn));
    std::snprintf(value, sizeof value, "%lld -> %lld", gamesNow, gamesIn);
    AddDialogLine("Games played", value, Compare(gamesNow, gamesIn));
    std::snprintf(value, sizeof value, "%lld -> %lld", killsNow, killsIn);
    AddDialogLine("Enemies killed", value, Compare(killsNow, killsIn));
    std::snprintf(value, sizeof value, "%u -> %u", bestNow, bestIn);
    AddDialogLine("Best score", value, Compare(bestNow, bestIn));
    std::snprintf(value, sizeof value, "%d -> %d", now.achievementsUnlocked, in.achievementsUnlocked);
    AddDialogLine("Achievements", value, Compare(now.achievementsUnlocked, in.achievementsUnlocked));
}

void SettingsScreen::SetDialogPressed(int button) {
    m_dialogPressed = button;
    for (int k = 0; k < 2; ++k) {
        m_dialogButtons[k].SetColour(k == button ? DivideColour(kRowColour, 2.0f) : kRowColour);
    }
}

// As the rows: a press highlights a button (with a click), the release on it acts.
void SettingsScreen::ProcessDialog() {
    Game* g = m_game;
    const GLfloat2 p{g->m_touchX, g->m_touchY};
    if (g->m_touchState == TOUCHING && m_lastTouchState == NOT_TOUCHING) {
        m_lastTouchState = TOUCHING;
        int pressed = -1;
        for (int k = 0; k < 2; ++k) {
            if (m_dialogButtons[k].ContainsPoint(p)) pressed = k;
        }
        if (pressed >= 0) g->QueueSound(SOUND_CLICK);
        SetDialogPressed(pressed);
        return;
    }
    if (m_lastTouchState != TOUCHING || g->m_touchState != NOT_TOUCHING) return;
    m_lastTouchState = NOT_TOUCHING;
    const int pressed = m_dialogPressed;
    SetDialogPressed(-1);
    if (pressed < 0 || !m_dialogButtons[pressed].ContainsPoint(p)) return;
    if (pressed == 0 && m_dialogKind == kDialogImport) {
        progress::Apply(*g, m_importData);
        g->ShowToast("Progress imported");
    } else if (pressed == 0 && m_dialogKind == kDialogReset) {
        progress::Reset(*g);
        g->ShowToast("Progress reset");
    }
    m_dialog = false;
}

void SettingsScreen::RenderDialog() {
    sdlDisable(GL_TEXTURE_2D);
    m_dialogShade.Render();
    sdlEnable(GL_TEXTURE_2D);
    sdlBindTexture(GL_TEXTURE_2D, 4);
    m_dialogPanel.Render();
    for (QuadStrip& b : m_dialogButtons) b.Render();
    m_dialogHeading.Render();
    for (Label& l : m_dialogLines) l.Render();
    for (Label& l : m_dialogButtonLabels) l.Render();
}
