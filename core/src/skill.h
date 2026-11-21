// Skill (derives from Upgrade). Layout: re/structs/Skill.json (0x204 bytes in the original).
// Two kinds share it: real skills (m_type < 0x2a: cooldown, charge, level) and the stat choices
// of a level-up (m_type = 0x2a + StatType, with the stat bonuses Game::SetSkill applies).
#pragma once

#include "dr_types.h"
#include "label.h"
#include "upgrade.h"

class Game;
class GameState;

class Skill : public Upgrade {
public:
    explicit Skill(Game* game);                              // 0x00060b60
    ~Skill() override;                                       // 0x00060b10

    void Initialise(SkillType type, Skill* previous);        // 0x00060984: previous = same skill one level lower
    void Initialise(StatType stat);                          // 0x000606a4
    void Activate();                                         // 0x00060210
    void Deactivate();                                       // 0x0005ff10
    bool CanActivate(int type);                              // 0x00060100
    bool CanActivate();                                      // 0x00060204: CanActivate(m_type)
    const char* GetDescription();                            // 0x0005fec4
    char* GetStats(int line);                                // 0x0005feec: m_statsText[line]
    void SetStatChance(int line, const char* label, int& bonus, int chance);   // 0x0005ff50
    void Serialise(GameState* state);                        // 0x0006009c
    void Deserialise(GameState* state);                      // 0x0005ffb0
    void RenderExtras() override;                            // 0x00060690: the level label (real skills)

    static const char* GetSkillName(SkillType type);         // 0x0005ff18: g_skillNames[type]
    static const char* GetSkillDescription(SkillType type);  // 0x0005ff2c
    static int GetBaseCooldown(SkillType type);              // 0x0005ff40
    static int GetStat(StatType stat);                       // 0x0005fef4
    static int GetStatChance();                              // 0x0005ff0c: 5
    static void GetIconUVs(SkillType type, GLfloat2& topLeft, GLfloat2& bottomRight);   // 0x000600cc

    Game* m_game;
    bool m_active;
    SkillType m_type;
    int m_level;
    int m_cooldown;                  // < 0: no cooldown
    int m_charge;                    // ready when == m_cooldown
    Label m_levelLabel;
    int m_statsTextCount;
    char m_statsText[4][64];
    int m_maxHealthBonus;
    int m_healAmountBonus;
    int m_armourAmountBonus;
    int m_baseDamageBonus;
    int m_coinTokenChanceBonus;
    int m_potionTokenChanceBonus;
    int m_skullTokenChanceBonus;
    int m_shieldTokenChanceBonus;

private:
    void PlaceLevelLabel();
};
