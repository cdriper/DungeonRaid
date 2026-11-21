// ClassScreen: the class details screen opened from the name screen (a class's race, perks, skills
// and abilities in four tabs). 0xd5c bytes in the original, embedded in NameScreen at +0x2c7c.
//
// Ported in three sub-steps (constructor; texts, tabs and updates; input and drawing). The a74..aac
// members are named by offset: a74 0 = waiting for a press, 1 = pressed; a78 the class/race frame
// pressed; a7c/a7d back/help button pressed; a80 the perk/flaw dragged (flaws + 10); a84 perk/flaw
// list entries that can be dragged; a98 the dragged frame's and icon's offsets from the touch;
// aa8 the skill dragged; aac skills that can be dragged.
#pragma once

#include <vector>

#include "dr_types.h"
#include "label.h"
#include "quad_strip.h"

class Game;

class ClassScreen {
public:
    explicit ClassScreen(Game* game);                   // 0x0003bbd4
    ClassScreen(const ClassScreen&) = delete;
    ClassScreen& operator=(const ClassScreen&) = delete;

    // AddPanel: a framed panel (3x3 quads of the frame at pixels (50,1)-(103,54)) of fixed size w x h
    // at (x, y), appended to the tab's strip list.
    void AddPanel(int tab, float w, float h, float x, float y);   // 0x0003b988

    void Initialise();                                  // 0x0003a264: shown; texts, unlocked icons, tab 0
    void FadeOut();                                     // 0x0003730c: keeps a valid current class, saves the classes
    void SetAlpha(unsigned alpha);                      // 0x00037a10: everything (the shade 6 at alpha / 3)
    void SelectTab(int tab);                            // 0x00039ba8: the tab's help text and colours
    void UpdateClass();                                 // 0x00039e68: class frames, skill levels, the class labels
    void UpdateRace();                                  // 0x00038d88
    void ShowRace(RaceType race);                       // 0x00038a60: race and mortal enemy texts
    void UpdatePerks();                                 // 0x000393e4
    void UpdateAbilities();                             // 0x00037670
    void ShowText(const char* title, const char* text, bool withIcon);   // 0x00038e98: the help text of the tab
    void ShowSkillText(SkillType skill);                // 0x000392a4
    void ShowFlawText(FlawType flaw);                   // 0x00039364
    void ShowPerkText(PerkType perk);                   // 0x000393a4
    void SetClassLevel(int level, int index, QuadStrip& frame);   // 0x000385ac: the level number in the frame's corner
    // Label `index` of `labels` (grown as needed; scale 0.09), count = max(count, index + 1).
    void SetText(int index, const char* text, std::vector<Label>& labels, int& count);   // 0x000387b8
    // Text wrapped at `width` in lines of `scale` from y (y < 0: below the last label), appended.
    void SetText(const char* text, float x, float y, float width, float scale, std::vector<Label>& labels,
                 int& count, GLcolour4 colour);         // 0x00038940
    void RenderTab(int tab);                            // 0x00037e68
    void Render();                                      // 0x00037f5c
    void Process(float dt);                             // 0x0003a598: fade, touches (tabs, choices, dragging)
    void GrabIcon(int tab, int frame, int icon, int item);   // 0x00037390: start dragging a perk/flaw/skill
    void QueueTabHelp();                                // port helper of Process: the help button's pages

    static constexpr int kStrips = 33;

    Game* m_game;                                       // 0x0
    // 0..3 the tabs, 4/5 bars, 6/7 black shades, 8 the content area, 9 and 10 buttons, 11..21 frames
    // for the icons 22..32 (22, 23, 32 big, 24..31 small).
    QuadStrip m_strips[kStrips];                        // 0x4 (0x4c each)
    int m_tab = 0;                                      // 0x9d0: the selected tab
    std::vector<Label> m_textLabels;                    // 0x9d4: the help text (and the perk descriptions 0..7)
    int m_textLabelCount = 0;                           // 0x9e0
    bool m_perkIcon = false;                            // 0x9e4: the perk tab's text has an icon (unset by the constructor)
    bool m_abilityIcon = false;                         // 0x9e5: the ability tab's text has an icon (unset)
    std::vector<Label> m_levelLabels;                   // 0x9e8: class levels in the frames' corners
    std::vector<Label> m_classLabels;                   // 0x9f4: 11 labels of the class tab
    std::vector<Label> m_raceLabels;                    // 0xa00: race and mortal enemy texts
    int m_raceLabelCount = 0;                           // 0xa0c (unset)
    std::vector<QuadStrip> m_panels[4];                 // 0xa10 per tab: frames, icons, panels
    std::vector<Label> m_tabLabels;                     // 0xa40: one per tab
    int m_abilityPanel = 0;                             // 0xa4c: index in m_panels[3]
    int m_perkIconsStart = 0;                           // 0xa50: index in m_panels[2]
    int m_perkPanel2 = 0;                               // 0xa54
    int m_perkPanel3 = 0;                               // 0xa58
    int m_racePanels = 0;                               // 0xa5c: index in m_panels[1]
    int m_raceIcons = 0;                                // 0xa60
    int m_classPanels = 0;                              // 0xa64: index in m_panels[0]
    int m_skillFrames = 0;                              // 0xa68
    bool m_active = false;                              // 0xa6c
    bool m_closing = false;                             // 0xa6d
    float m_fade = 0.0f;                                // 0xa70
    int m_a74 = 0;                                      // 0xa74 (Initialise: 1)
    int m_a78 = -1;                                     // 0xa78
    bool m_a7c = false;                                 // 0xa7c
    bool m_a7d = false;                                 // 0xa7d
    int m_a80 = -1;                                     // 0xa80
    uint8_t m_a84[0x14] = {};                           // 0xa84 (unset)
    float m_a98[4] = {};                                // 0xa98 (unset)
    int m_aa8 = -1;                                     // 0xaa8
    uint8_t m_aac[0x18] = {};                           // 0xaac (unset)
    QuadStrip m_grabbedIcon;                            // 0xac4
    QuadStrip m_grabbedSmallIcon;                       // 0xb10
    char m_text[0x200] = {};                            // 0xb5c: text buffer (help texts, labels; unset)
};
