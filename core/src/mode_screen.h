// ModeScreen: the challenge mode chooser (Dungeon Sprint or Pretzel Hero): two mode buttons, the
// chosen mode's icon, description, scoring and special rules, a back button. 0x50c bytes in the
// original, embedded in NameScreen at 0x39d8 (Game 0x13dbc); the layout below follows it. Until
// NameScreen is ported it is a Game member of its own (Game::m_modeScreen).
#pragma once

#include <vector>

#include "label.h"
#include "quad.h"
#include "quad_strip.h"

class Game;

// GLcolour4::operator/(float) (0x000545e4): r, g, b divided (unsigned conversion), alpha kept
GLcolour4 DivideColour(const GLcolour4& c, float d);

class ModeScreen {
public:
    explicit ModeScreen(Game* game);                    // 0x000550e8
    ~ModeScreen() = default;                            // 0x000566a0
    ModeScreen(const ModeScreen&) = delete;
    ModeScreen& operator=(const ModeScreen&) = delete;

    void Initialise();                                  // 0x0005474c: the three headings
    void Show();                                        // 0x00054e68
    void FadeOut();                                     // 0x00054650
    void UpdateMode();                                  // 0x00055dec: follows Game::m_mode
    void ShowMode();                                    // 0x00055be4
    void SetAlpha(uint8_t alpha);                       // 0x00054d4c
    void Process(float dt);                             // 0x00054e8c
    void Render();                                      // 0x00054fe8

    static void MakeButton(QuadStrip& strip, float w, float h, float x, float y);   // 0x00054794
    // (ScoreScreen::MakePanel 0x0005c618 is the same with a 0.0375 heading bar)
    static void MakePanel(QuadStrip& strip, bool heading, float w, float h, float x, float y,
                          float headingHeight = 0.05625f);   // 0x00054954
    void MakeHeadingLabel(QuadStrip& panel, Label& label, const char* text);         // 0x000546c8
    void AddLabel(const char* text, QuadStrip& panel, std::vector<Label>& labels, int& count, float indent);   // 0x00055918

    Game* m_game;                                       // 0x0
    bool m_active = false;                              // 0x4
    bool m_closing = false;                             // 0x5 (unset by the constructor in the original, as those marked so)
    float m_fadeTime = 0.0f;                            // 0x8 (unset)
    int m_lastTouchState = 0;                           // 0xc (unset)
    Quad m_background;                                  // 0x10: black
    Quad m_panelBackground;                             // 0x84
    QuadStrip m_backButton;                             // 0xf8
    bool m_backPressed = false;                         // 0x144 (unset)
    QuadStrip m_modePanel;                              // 0x148
    Label m_modeHeading;                                // 0x194
    QuadStrip m_modeButtons[2];                         // 0x1e0
    QuadStrip m_modeButtonIcons[2];                     // 0x278
    int m_shownMode = 2;                                // 0x310: 2 = none yet
    QuadStrip m_descriptionPanel;                       // 0x314
    QuadStrip m_modeIcon;                               // 0x360
    std::vector<Label> m_descriptionLabels;             // 0x3ac
    int m_descriptionCount = 0;                         // 0x3b8
    QuadStrip m_scoringPanel;                           // 0x3bc
    Label m_scoringHeading;                             // 0x408
    std::vector<Label> m_scoringLabels;                 // 0x454
    int m_scoringCount = 0;                             // 0x460
    QuadStrip m_rulesPanel;                             // 0x464
    Label m_rulesHeading;                               // 0x4b0
    std::vector<Label> m_rulesLabels;                   // 0x4fc
    int m_rulesCount = 0;                               // 0x508
};
