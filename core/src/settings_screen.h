// SettingsScreen (port-only, not in the original): the game's settings on a screen of their own,
// opened from the title screen or the pause menu (GameMenu), which in the original held them as
// tick boxes. A titled panel with one row per setting and a Back button, in the style of ModeScreen
// (texture 4 frames, the game's fonts, texture 1 tick boxes). Two kinds of row:
//  - a toggle: name and tick box; a tap toggles it;
//  - a slider (0..100 in steps of 5): name, track, knob and value; the knob follows a press and drag
//    on the track, the value is set on release (then a click at the new level).
//  - an action: just the name; a tap runs it (Export progress, Import progress).
//  - a choice: name and the value's name on the right; a tap steps to the next value (Grind,
//    Info panel).
// A row can be shown only while a Game flag is set (the Debug mode row: only in debug mode); the
// rows are laid out again when the screen opens with a different set.
// Page 1 (debug mode, the pause menu's Debug button) is the Debug page: Add skill, Add special
// monster, Level up, Shop, Armour upgrade (these three close it and the pause menu first), Add
// achievement (the first locked one unlocks), Unlock all (every class at level 10).
// Progress (progress_code.h): Export puts a code on the clipboard (via the host); Import (title
// screen only) has the host read the clipboard into ReceiveImport, which shows an error toast or a
// dialog comparing the current and the imported progress, with Import / Cancel buttons. Reset
// progress (title screen only) asks Yes / No, then wipes the progress (progress::Reset).
// Fades in over what is showing and back out; while open it takes the frames and the back key
// (Game::Process, Game::BackPressed).
//
// The settings are the table kSettings in settings_screen.cpp: a new one is one more row there.
#pragma once

#include <string>
#include <vector>

#include "label.h"
#include "progress_code.h"
#include "quad.h"
#include "quad_strip.h"

class Game;

class SettingsScreen {
public:
    explicit SettingsScreen(Game* game);
    ~SettingsScreen() = default;
    SettingsScreen(const SettingsScreen&) = delete;
    SettingsScreen& operator=(const SettingsScreen&) = delete;

    void Show(int page = 0);   // fades in (the fonts must be loaded); page 1: the Debug page
    void FadeOut();   // fades out, then inactive
    void Back();      // the back key: closes the import dialog, else the screen
    void Process(float dt);
    void Render();
    // The clipboard's text after an Import (the host calls it): an error toast, or the dialog.
    void ReceiveImport(const std::string& text);

    bool m_active = false;
    bool m_dialog = false;   // the import dialog is open
    const Quad& Background() const { return m_background; }   // black over the screen (its alpha fades)

private:
    struct Row {
        int setting = 0;      // index into kSettings
        QuadStrip button;
        Label label;
        Quad tick;            // toggle
        Quad track;           // slider: the whole track (untextured)
        Quad fill;            // slider: the part up to the knob (untextured)
        QuadStrip knob;       // slider
        Label value;          // slider: "75%"; choice: the value's name
        float trackX = 0.0f;  // slider: track's left end and width (screen units)
        float trackW = 0.0f;
        float valueRight = 0.0f;   // choice: the value's right edge
        int shown = -1;       // slider: the value the knob shows; choice: the value shown
    };

    std::vector<int> VisibleSettings() const;           // indices into kSettings
    void Layout(const std::vector<int>& visible);       // the panel and the rows (the fonts are loaded)
    int SettingIndex(int row) const { return m_rows[static_cast<size_t>(row)].setting; }
    void SetAlpha(uint8_t alpha);
    void UpdateRows();                     // follow the Game's values (not a slider being dragged)
    void ShowSliderValue(int row, int value);
    void ShowChoice(int row, int value);
    int SliderValueAt(int row, float x) const;
    int RowAt(const GLfloat2& p);          // -1: none
    void RunAction(int action);
    void OpenDialog();                     // lays out the comparison of m_importData with the game's
    void OpenResetDialog();                // Reset progress: Yes / No
    void MakeDialog(const char* heading, float height, const char* yes, const char* no);   // panel, buttons
    void ProcessDialog();
    void RenderDialog();
    void SetDialogPressed(int button);
    Label& AddDialogLine(const char* name, const char* value, int change);   // change: <0 red, >0 green

    Game* m_game;
    int m_page = 0;                        // 0 settings, 1 debug (the table the rows index)
    std::vector<int> m_layout;             // the settings the rows show
    bool m_closing = false;
    float m_fadeTime = 0.0f;
    int m_lastTouchState = 0;
    int m_pressed = -1;                    // a row, kBack, or -1
    uint8_t m_alpha = 0;
    Quad m_background;                     // black, the whole screen
    Quad m_panelBackground;                // dark blue, above the Back button
    QuadStrip m_panel;
    Label m_heading;
    QuadStrip m_backButton;
    std::vector<Row> m_rows;
    // the import dialog
    progress::Data m_importData;
    int m_dialogPressed = -1;              // 0 Import / Yes, 1 Cancel / No, -1 none
    static constexpr int kDialogImport = 0;
    static constexpr int kDialogReset = 1;
    int m_dialogKind = kDialogImport;      // which dialog is open
    Quad m_dialogShade;                    // dims the settings behind
    QuadStrip m_dialogPanel;
    Label m_dialogHeading;
    std::vector<Label> m_dialogLines;      // name and value labels
    QuadStrip m_dialogButtons[2];
    Label m_dialogButtonLabels[2];
};
