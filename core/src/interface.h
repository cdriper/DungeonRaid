// Interface: the in-game HUD (bars, values, skill buttons, menus). Layout: re/structs/Interface.json
// (0x1288 bytes in the original, embedded in Game at 0x4cec); members in the original's order.
//
// Fully ported, with its menus (FoldoutMenu, GameMenu, StatsMenu) and the Tooltip.
#pragma once

#include "dr_types.h"
#include "foldout_menu.h"
#include "game_menu.h"
#include "label.h"
#include "quad.h"
#include "stats_menu.h"
#include "tooltip.h"

class Game;
class Token;

class Interface {
public:
    // Which bar an Update*Bar call refers to (the original passes a reference to the bar's Quad;
    // Game always passes the main bars, the Set*shadow methods the shadow bars).
    enum Bar { BAR_HEALTH, BAR_XP, BAR_UPGRADE, BAR_GOLD };

    explicit Interface(Game* game);                     // 0x0004aadc
    Interface(const Interface&) = delete;
    Interface& operator=(const Interface&) = delete;

    // The bars: size, texture rect and position from the value; each also moves its token target
    // (the targets are where collected tokens fly, so they decide when a token arrives).
    void UpdateHealthBar(Quad& bar, int value);         // 0x0004a03c (also the "health/max" label)
    void UpdateXPBar(Quad& bar, int value);             // 0x00049e34
    void UpdateUPBar(Quad& bar, int value);             // 0x00049f40
    void UpdateGoldBar(Quad (&bars)[2], int value);     // 0x0004a1dc: a bar per 20 gold and one for the rest
    void UpdateHealthBar(Bar, int value) { UpdateHealthBar(m_healthBar, value); }
    void UpdateXPBar(Bar, int value) { UpdateXPBar(m_xpBar, value); }
    void UpdateUPBar(Bar, int value) { UpdateUPBar(m_upgradeBar, value); }
    void UpdateGoldBar(Bar, int value) { UpdateGoldBar(m_goldBar, value); }
    void UpdateShieldTarget();                          // 0x00048f3c
    void UpdateShieldValue();                           // 0x00048fa8: the "armour/max" label, shield target
    void UpdateSwordValue();                            // 0x0004893c: "+n" over the sword icon
    void UpdateSkullValue();                            // 0x000488cc
    void UpdateHealthColour();                          // 0x00048c38: heartbeat flash, purple when poisoned
    void UpdateSkills();                                // 0x00049544: charge bars, ready glows (+ their help pages)
    void PositionSkill(int index);                      // 0x0004871c: the skill's icon on its button
    void SetPoison(int poison);                         // 0x00048d80
    void SetClockState(bool visible);                   // 0x000487dc
    void SetClockString(const char* text);              // 0x0004887c
    // previews of what the current chain would add (value < 1: none)
    void SetXPshadow(int value);                        // 0x00049f04
    void SetGoldShadow(int value);                      // 0x0004a3d4
    void SetHealthShadow(int value);                    // 0x0004a19c
    void SetUPshadow(int value);                        // 0x0004a000
    void Reset();                                       // 0x00048b14 (new game)

    GLfloat2* GetTarget(TileType type);                 // 0x00049028: where tokens of a type fly to
    void SetHealth(int health);                         // 0x00024d0c
    int GetSpecialTarget(Token* token);                 // 0x0004b254: skill a mana token charges, -1 none
    // Position of skill button `index` (0..3): g_topButtonPositions (static-initialised, values read
    // from the original).
    static GLfloat2 GetSpecialTarget(int index);        // 0x0004875c
    static GLfloat2 GetPoisonPosition() { return {0.853125f, 1.384375f}; }   // 0x00048774
    void ShowMenu(int menu);                            // 0x00048f30 (MenuType): FoldoutMenu::Initialise
    // 0x00049988: the fold-out menu, else the game/stats menus, the HUD's input or the heartbeat;
    // then the tooltip, the skill-ready glows and (challenges) the score tally and labels.
    void Process(float dt, bool inputReady);
    // 0x00049738: the skill buttons (press on a charged skill, release on it: Skill::Activate; the
    // skill's tooltip), the menu and stats buttons.
    void ProcessInput();
    bool CursorInButton(int index) const;               // 0x000486a4: skill button `index`
    void ProcessHeartbeat(float dt);                    // 0x00048dfc: low health
    void Render();                                      // 0x000490ec
    void RenderMenu();                                  // 0x000490b4
    void RenderTooltip();                               // 0x0004909c
    // the help pages about the HUD (HelpMenu::Initialise at the item)
    void ShowHelpXP();                                  // 0x000489e4
    void ShowHelpUP();                                  // 0x00048a14
    void ShowHelpWeaponDmg();                           // 0x00048a44
    void ShowHelpArmour();                              // 0x00048a78
    void ShowHelpBaseDmg();                             // 0x00048aac
    void ShowHelpGold();                                // 0x00048ae0
    void ShowHelpHealth();                              // 0x000489b0

    // the tooltip's entry points as the game calls them (m_tooltip)
    void HideTooltip() { m_tooltip.Hide(); }
    void HideTooltipInstantly() { m_tooltip.HideInstantly(); }
    void ShowTooltip(int enemyType, const GLfloat2& position) { m_tooltip.Show(static_cast<EnemyType>(enemyType), position); }
    void ShowScrollTooltip(int skillType, const GLfloat2& position) { m_tooltip.ShowScroll(static_cast<SkillType>(skillType), position); }
    void ShowSkillTooltip(int skillType, int cooldown, int charge, const GLfloat2& position) {
        m_tooltip.Show(static_cast<SkillType>(skillType), cooldown, charge, position);
    }

    Quad m_bottomPanel;                                 // 0x0
    Quad m_healthBar;                                   // 0x74
    Quad m_xpBar;                                       // 0xe8
    Quad m_upgradeBar;                                  // 0x15c
    Quad m_goldBar[2];                                  // 0x1d0
    Quad m_healthShadowBar;                             // 0x2b8 (alpha 0x78)
    Quad m_xpShadowBar;                                 // 0x32c (alpha 0x50)
    Quad m_upgradeShadowBar;                            // 0x3a0 (alpha 0x78)
    Quad m_goldShadowBar[2];                            // 0x414 (alpha 0x50)
    Label m_swordLabel;                                 // 0x4fc
    Label m_shieldLabel;                                // 0x548
    Label m_skullLabel;                                 // 0x594
    Label m_healthLabel;                                // 0x5e0
    Label m_poisonLabel;                                // 0x62c
    Label m_clockLabel;                                 // 0x678
    Quad m_topPanel;                                    // 0x6c4
    Quad m_skillButtons[4];                             // 0x738
    Quad m_skillChargeBars[4];                          // 0x908
    Quad m_skillReadyGlows[4];                          // 0xad8
    int m_pressedSkill = -1;                            // 0xca8: skill button being pressed, -1 none
    Quad m_menuButton;                                  // 0xcac
    Quad m_statsButton;                                 // 0xd20
    bool m_menuButtonPressed = false;                   // 0xd94
    bool m_statsButtonPressed = false;                  // 0xd95
    // values; the constructor sets them to -1 (= nothing shown yet)
    int m_health = -1;                                  // 0xd98
    int m_maxHealth = -1;                               // 0xd9c
    int m_xp = -1;                                      // 0xda0
    int m_upgradePoints = -1;                           // 0xda4
    int m_gold = -1;                                    // 0xda8
    int m_poison = 0;                                   // 0xdac (unset by the constructor in the original)
    int m_healthShadow = -1;                            // 0xdb0: shown shadow values, -1 none
    int m_xpShadow = -1;                                // 0xdb4
    int m_upgradeShadow = -1;                           // 0xdb8
    int m_goldShadow = -1;                              // 0xdbc
    int m_swordValue = -1;                              // 0xdc0
    int m_armour = -1;                                  // 0xdc4
    int m_maxArmour = -1;                               // 0xdc8
    int m_skullValue = -1;                              // 0xdcc
    // token targets: the constructor sets only health x and XP y (the bars set the rest)
    GLfloat2 m_goldTarget{0.0f, 0.0f};                  // 0xdd0
    GLfloat2 m_xpTarget{0.0f, 1.4625f};                 // 0xdd8
    GLfloat2 m_shieldTarget{0.0f, 0.0f};                // 0xde0
    GLfloat2 m_healthTarget{0.853125f, 0.0f};           // 0xde8
    FoldoutMenu m_foldoutMenu;                          // 0xdf0
    GameMenu m_gameMenu;                                // 0xf08
    StatsMenu m_statsMenu;                              // 0xf9c
    Game* m_game;                                       // 0x1144
    int m_lastTouchState = 0;                           // 0x1148 (TouchState) at the last ProcessInput
    Tooltip m_tooltip;                                  // 0x114c
    float m_heartbeatPulse = 0.0f;                      // 0x11c4: time left of the current beat
    float m_heartbeatPulseLength = 1.0f;                // 0x11c8
    float m_heartbeatDelay = 0.0f;                      // 0x11cc: time to the next beat
    float m_heartbeatColour = 0.0f;                     // 0x11d0
    bool m_clockVisible = false;                        // 0x11d4 (unset by the constructor in the original)
    Label m_scoreLabel;                                 // 0x11d8 (challenges)
    Label m_turnLabel;                                  // 0x1224 (Sprint)
    int m_displayedScore = 0;                           // 0x1270 (the score tally; unset by the constructor)
    int m_scoreGain = 0;                                // 0x1274
    int m_scoreBonus = 0;                               // 0x1278
    bool m_scoreTallying = false;                       // 0x127c
    float m_scoreTallyDelay = 0.0f;                     // 0x1280
    float m_scoreTallyStep = 0.0f;                      // 0x1284
};

