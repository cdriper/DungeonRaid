// FoldoutMenu: the menu that folds out from the left for a level-up (pick 2 of 4: new skills,
// skill levels or stat bonuses), the shop (buy 1 of 3 items, one may come with a free skill
// scroll) or an upgrade (1 of 4 item upgrades). 0x118 bytes in the original, embedded in
// Interface at 0xdf0; the layout below follows it (offsets in the comments).
//
// Fully ported: the offers (drawn from the RNG), touch handling, the fold animation, applying the
// choice, the text labels (each option's lines; in the shop the item an offer would replace, with
// the stat texts coloured by comparison) and Render.
#pragma once

#include <vector>

#include "dr_types.h"
#include "menu.h"
#include "quad.h"

class Game;
class Interface;
class Item;
class ItemUpgrade;
class Label;
class Skill;

class FoldoutMenu : public Menu {
public:
    // Quads: 0..6 frame, 7 confirm button, 8..13 selection marks (two rows of 3), 14..17 option
    // buttons, 18/19 title, 20..22 separator, 23.. scroll icon (shop) / upgrade icons, 27..30 arrows,
    // 31..33 (port) the right side of the frame (horizontal layout only).
    static constexpr int kQuadCount = 0x22;
    static constexpr int kRightFrame = 0x1f;
    static constexpr int kConfirmQuad = 7;
    static constexpr int kFirstOption = 14;

    FoldoutMenu();                                      // the Menu part of the constructor (vertex array)
    ~FoldoutMenu() override;                            // 0x00042218
    // The rest of FoldoutMenu(Game*, Interface*) (0x000410e0): quads, offer objects, shade. Called
    // by Game's constructor, where the original constructs Interface.
    void Create(Game* game, Interface* ui);

    void Initialise(int type);                          // 0x00042794 (MenuType): 0 level-up, 1 shop, 2 upgrade
    void Process(float dt);                             // 0x00041fc4
    void ProcessInput();                                // 0x00041d54
    bool CursorInButton(int quad) const;                // 0x000406f4 (no right-edge test)
    bool CursorInQuad(int quad) const;                  // 0x00040674
    void HighlightButton(int quad, int row, bool on);   // 0x00040cb0
    void AddLabel(int option, const char* text);        // 0x000422fc
    void UpdateLabels();                                // 0x000407b8
    void Render();                                      // 0x00040eb4
    // port: x of the open menu: 0.071875 (its right side at the screen's edge); on the horizontal
    // layout 13 units further left, so the right side of the frame (kRightFrame) fits the screen
    float RestX() const;

    // (scalars the original constructor leaves unset are 0, as for the other classes)
    bool m_active = false;                              // 0x8
    int m_type = 0;                                     // 0xc (MenuType)
    Game* m_game = nullptr;                             // 0x10
    Interface* m_interface = nullptr;                   // 0x14
    GLfloat2 m_offset{0.0f, 0.0f};                      // 0x18: added to every quad (x folds 1.0 <-> 0.071875)
    bool m_confirmEnabled = false;                      // 0x20: enough options selected
    bool m_confirmPressed = false;                      // 0x21
    float m_animTime = 0.0f;                            // 0x24: fold animation time, -1 = idle (input)
    bool m_opening = false;                             // 0x28: folding out (true) or in
    int m_lastTouchState = 0;                           // 0x2c: Game touch state at the last ProcessInput
    int m_selected[2] = {0, 0};                         // 0x30: selected option quads (14..17), -1 none
    Item* m_items[4] = {};                              // 0x38: shop offers (slot 3 unused)
    Skill* m_skills[4] = {};                            // 0x48: level-up offers
    Item* m_upgradeItems[4] = {};                       // 0x58: the player's items the upgrades are for
    ItemUpgrade* m_upgrades[4] = {};                    // 0x68
    int m_skillOfferCount = 0;                          // 0x78: level-up options that are skills (the rest stats)
    std::vector<Label*> m_labels;                       // 0x7c: grows as needed, never shrinks
    int m_labelCount = 0;                               // 0x88: labels in use
    int m_optionLabelCounts[4] = {0, 0, 0, 0};          // 0x8c: labels in use per option
    Quad m_shade;                                       // 0x9c: darkens the board while open
    int m_scrollIndex = 0;                              // 0x110: shop offer with a scroll, -1 none
    Skill* m_scrollSkill = nullptr;                     // 0x114
};

