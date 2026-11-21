// MainMenu: the title screen: background, logo, buttons (continue, new game, challenge, tutorial,
// scores, sound, feint, bonus), the "abandon the saved game?" confirmation, fades. 0x684 bytes in
// the original, embedded in Game at 0xc7dc; the layout below follows it.
//
// Its buttons start the Game methods ContinueGame, NewGame, StartChallenge, StartTutorial and the score screen.
#pragma once

#include "label.h"
#include "quad.h"

class Game;

class MainMenu {
public:
    // buttons: 0 continue, 1 new game, 2 challenge, 3 tutorial, 4 scores, 5 sound, 6 feint, 7 bonus
    static constexpr int kButtonCount = 8;

    explicit MainMenu(Game* game);                      // 0x00053384
    ~MainMenu() = default;                              // 0x000532dc
    MainMenu(const MainMenu&) = delete;
    MainMenu& operator=(const MainMenu&) = delete;

    void Show();                                        // 0x00052bdc
    void SetActive(bool active);                        // 0x00052b90
    void FadeIn();                                      // 0x00052ba0
    void FadeOut();                                     // 0x00052b7c
    void SetSoundState(bool on);                        // 0x00053fb4
    void Process(float dt);                             // 0x00052c54
    void Render();                                      // 0x00053238

    Game* m_game;                                       // 0x0
    bool m_active = true;                               // 0x4
    float m_fadeTime = 0.0f;                            // 0x8
    bool m_fadingIn = false;                            // 0xc (unset by the constructor in the original, as the others marked so)
    Quad m_background;                                  // 0x10
    Quad m_logo;                                        // 0x84: fades out when the menu starts (500/s)
    Quad m_buttons[kButtonCount];                       // 0xf8
    int m_pressed = -1;                                 // 0x498
    bool m_canContinue = false;                         // 0x49c: a saved game (unset)
    bool m_hasScores = false;                           // 0x49d (unset)
    bool m_bonusVisible = false;                        // 0x49e
    Quad m_confirmShade;                                // 0x4a0
    Quad m_confirmPanel;                                // 0x514
    Quad m_confirmYes;                                  // 0x588
    Quad m_confirmNo;                                   // 0x5fc
    bool m_confirming = false;                          // 0x670
    int m_confirmAction = 0;                            // 0x674: 0 new game, 1 challenge, 2 tutorial (unset)
    bool m_confirmYesChosen = false;                    // 0x678 (unset)
    bool m_confirmClosing = false;                      // 0x679
    float m_confirmTime = 0.0f;                         // 0x67c (unset)
    int m_lastTouchState = 0;                           // 0x680
    Label m_versionLabel;                               // port: version and build time, bottom right
    Quad m_achievementsButton;                          // port: under Scores (OpenFeint's place); opens AchievementsScreen
    Quad m_settingsButton;                              // port: under Achievements; opens SettingsScreen
    Quad m_exitButton;                                  // port: desktop only (Game::m_desktop), where Bonus Item was; quits
    int m_versionTaps = 0;                              // port: taps on the build info in a row (3 toggle debug mode)
    double m_versionTapTime = 0.0;                      // port: the last one (GetTime)

private:
    static constexpr int kSettingsPressed = kButtonCount;   // port: m_pressed for m_settingsButton
    static constexpr int kAchievementsPressed = kButtonCount + 1;   // port: for m_achievementsButton
    static constexpr int kExitPressed = kButtonCount + 2;   // port: for m_exitButton
    bool ExitShown() const;                          // port: the desktop has the Exit button
    int PortButtonAt(const GLfloat2& p) const;       // port: the port button (k*Pressed) under p, -1 if none or Scores wins
    void SetPortAlphas(uint8_t alpha);               // port: the port buttons' alpha
    bool VersionTapped(const GLfloat2& p);           // port: counts taps on the build info; true when handled
    void SetButtonAlphas(int alpha, uint8_t full);   // the dimmed buttons (no saved game, no scores) at alpha / 2
    void StartAction(int action);                    // 0 new game, 1 challenge, 2 tutorial
};
