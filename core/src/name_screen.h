// NameScreen: the new game setup: the character name (an on-screen keyboard, or a random name),
// the difficulty (or, for a challenge, the mode, with ModeScreen as its help), the class (the ten
// classes, their levels, the chosen class's race, perks, flaws and skill), Begin. 0x3ee8 bytes in
// the original, embedded in Game at 0x103e4; the layout below follows it. It holds ClassScreen
// (+0x2c7c, class_screen.h) and ModeScreen (+0x39d8).
#pragma once

#include "class_screen.h"
#include "label.h"
#include "mode_screen.h"
#include "quad.h"
#include "quad_strip.h"

class CharacterClass;
class Game;


class NameScreen {
public:
    static constexpr int kKeys = 30;   // 0..25 letters, 26 shift, 27 delete, 28 space, 29 OK

    explicit NameScreen(Game* game);                    // 0x00058c3c
    ~NameScreen();                                      // 0x00056748
    NameScreen(const NameScreen&) = delete;
    NameScreen& operator=(const NameScreen&) = delete;

    void Initialise();                                  // 0x00056490: texts
    void Show(const char* name, bool challenge);        // 0x000589a0
    void FadeOut();                                     // 0x00055ebc
    void SelectClass(CharacterClass& c);                // 0x00057694
    void SetAlpha(int alpha);                           // 0x00056234
    void Process(float dt);                             // 0x00058704
    void ProcessInput(float dt);                        // 0x00057a10
    void Render();                                      // 0x0005726c
    void UpdateName();                                  // 0x00056090
    void UpdateDifficulty();                            // 0x00056b60
    void UpdateMode();                                  // 0x000571bc
    void GenerateRandomName();                          // 0x0005615c
    void MakeHeadingLabel(QuadStrip& panel, Label& label, const char* text);   // 0x00055f3c
    void MakeButtonLabel(QuadStrip& button, Label& label, const char* text);   // 0x00055fc0

    // Port-only: a physical keyboard while the on-screen keyboard is up (desktop hosts). Same rules
    // as the on-screen keys in ProcessInput: letters and space only, 16 characters, the first letter
    // capitalised (m_caps), the click sound; Enter acts as OK.
    bool AcceptsTyping() const;
    void TypeCharacter(char c);
    void TypeBackspace();
    void TypeEnter();

    Game* m_game;                                       // 0x0
    Quad m_background;                                  // 0x4
    Quad m_panelBackground;                             // 0x78
    Quad m_keyboardBackground;                          // 0xec
    QuadStrip m_backButton;                             // 0x160
    Label m_backLabel;                                  // 0x1ac
    QuadStrip m_namePanel;                              // 0x1f8
    Label m_nameHeading;                                // 0x244
    QuadStrip m_nameField;                              // 0x290
    Label m_nameLabel;                                  // 0x2dc
    QuadStrip m_randomButton;                           // 0x328
    Label m_randomLabel;                                // 0x374
    QuadStrip m_difficultyPanel;                        // 0x3c0 (also the mode panel)
    Label m_difficultyHeading;                          // 0x40c
    QuadStrip m_difficultyButtons[4];                   // 0x458
    Label m_difficultyLabels[4];                        // 0x588
    Label m_modeHeading;                                // 0x6b8
    QuadStrip m_modeHelpButton;                         // 0x704
    Label m_modeHelpLabel;                              // 0x750
    QuadStrip m_modeButtons[2];                         // 0x79c
    QuadStrip m_modeIcons[2];                           // 0x834
    QuadStrip m_classPanel;                             // 0x8cc
    Label m_classHeading;                               // 0x918
    QuadStrip m_classHelpButton;                        // 0x964
    Label m_classHelpLabel;                             // 0x9b0
    QuadStrip m_classButtons[10];                       // 0x9fc
    Quad m_classIcons[10];                              // 0xcf4
    Label m_classLevels[10];                            // 0x117c
    QuadStrip m_propertiesPanel;                        // 0x1474
    Label m_propertiesHeading;                          // 0x14c0
    QuadStrip m_editButton;                             // 0x150c
    Label m_editLabel;                                  // 0x1558
    QuadStrip m_propertySlots[11];                      // 0x15a4: class, race, perks/flaws (2..9), skill
    Quad m_propertyIcons[11];                           // 0x18e8
    Quad m_keys[kKeys];                                 // 0x1de4
    Label* m_keyLabels[kKeys] = {};                     // 0x2b7c
    float m_keyboardT = 0.0f;                           // 0x2bf4: keyboard slide (1 = shown)
    bool m_editing = false;                             // 0x2bf8: the keyboard is up
    bool m_active = false;                              // 0x2bf9
    float m_fadeTime = 0.0f;                            // 0x2bfc
    bool m_closing = false;                             // 0x2c00
    char m_name[19] = {};                               // 0x2c01
    int m_nameLength = 0;                               // 0x2c14
    bool m_named = false;                               // 0x2c18: a name was given (not the "Enter name" hint)
    float m_blink = 0.0f;                               // 0x2c1c: cursor
    float m_deleteTimer = 0.0f;                         // 0x2c20: delete key repeat
    bool m_caps = true;                                 // 0x2c24
    int m_pressed = -1;                                 // 0x2c28: key (0..29) or button (0x1e random, 0x1f edit, 0x20 class help, 0x21 back, 0x22 mode help)
    int m_lastTouchState = 0;                           // 0x2c2c
    Label m_begin;                                      // 0x2c30
    ClassScreen m_classScreen;                          // 0x2c7c
    ModeScreen m_modeScreen;                            // 0x39d8
    bool m_challenge = false;                           // 0x3ee4
};
