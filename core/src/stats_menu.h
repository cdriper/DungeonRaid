// StatsMenu: the character sheet behind the HUD's stats button: the stats (name and value labels,
// green where items or levels add to them), the difficulty or mode, the score, the five items
// and the class icon as buttons; tapping one shows its stat lines (or the race's bonus, perks and
// flaws) below, growing the menu. 0x1a8 bytes in the original, embedded in Interface at 0xf9c;
// the layout below follows it (offsets in the comments).
#pragma once

#include <vector>

#include "label.h"
#include "menu.h"
#include "quad.h"
#include "quad_strip.h"

class Game;

class StatsMenu : public Menu {
public:
    // Quads: 0..8 frame, 9 the info panel (shown while it has lines), 10..14 item buttons (helmet,
    // armour, weapon, shield, accessory), 15 the class button.
    static constexpr int kQuadCount = 0x10;
    static constexpr int kStatCount = 17;

    explicit StatsMenu(Game* game);                     // 0x000618f0
    ~StatsMenu() override;                              // 0x000627ac

    void Show();                                        // 0x000614bc
    void Process(float dt);                             // 0x00061d84
    void Render();                                      // 0x00060fdc
    void CreateLabels();                                // 0x00061178
    void SetLabelValue(int stat, int base, int bonus, bool percent);   // 0x00060f08
    void SetMenuHeight(int height);                     // 0x00060c74: pixels below the stats
    GLfloat2 GetButtonPosition(int quad) const;         // 0x00060be0
    bool CursorInQuad(int quad) const;                  // 0x00060c0c
    bool CursorOutsideMenu() const;                     // 0x00061d08

    Game* m_game;                                       // 0x8
    bool m_active = false;                              // 0xc
    int m_lastTouchState = 0;                           // 0x10 (unset by the constructor in the original)
    int m_selected = 0;                                 // 0x14: selected button, -1 none (likewise)
    bool m_closing = false;                             // 0x18 (likewise)
    float m_fadeTime = 0.0f;                            // 0x1c: fade time left (0.2 s), 0 = idle (likewise)
    Quad m_shade;                                       // 0x20
    QuadStrip m_classIcon;                              // 0x94
    std::vector<Label*> m_nameLabels;                   // 0xe0: 17 (CreateLabels)
    std::vector<Label*> m_valueLabels;                  // 0xec: 17
    std::vector<Label*> m_infoLabels;                   // 0xf8: 15, the selected button's lines
    Label m_titleLabel;                                 // 0x104: difficulty or mode
    Label m_scoreLabel;                                 // 0x150
    int m_infoCount = 0;                                // 0x19c: info lines shown
    float m_valueRight = 0.0f;                          // 0x1a0: right edge of the values
    float m_height = 0.0f;                              // 0x1a4

private:
    void SetIconAlpha(uint8_t alpha);   // the five items' icons
};
