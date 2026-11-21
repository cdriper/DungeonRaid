// CharacterClass: one of the 10 classes (Game::m_classes): race, perks/flaws, skill lists, level
// and per-class statistics. Layout: re/structs/CharacterClass.json.
#pragma once

#include <set>
#include <vector>

#include "dr_types.h"

class Game;
class GameState;

class CharacterClass {
public:
    CharacterClass();                                       // 0x00035b90
    ~CharacterClass();                                      // 0x000371b0

    void Initialise(ClassType type, Game* game);            // 0x00036524
    void SerialiseGameState(GameState* state);              // 0x000361ac
    void DeserialiseGameState(GameState* state);            // 0x00036168
    void SerialiseFullState(GameState* state);              // 0x00036494
    void DeserialiseFullState(GameState* state);            // 0x0003643c
    void Validate();                                        // 0x00036390

    bool IsPerkUnlocked(int slot) const;                    // 0x00035d78
    bool HavePerk(PerkType perk) const;                     // 0x00035dec
    bool HaveFlaw(FlawType flaw) const;                     // 0x00035e40
    void SetPerk(int slot, PerkType perk);                  // 0x00035e18
    void SetFlaw(int slot, FlawType flaw);                  // 0x00035e6c
    int GetPerkCount() const;                               // 0x00035f4c
    int GetFlawCount() const;                               // 0x00035f74
    void SetLevel(int level);                               // 0x00035f9c
    void LevelUp();                                         // 0x0003600c
    void UnlockAchievement();                               // 0x00035fb0
    const char* GetLevelDescription(int level);             // 0x0003602c

    bool IsBaseSkill(SkillType skill) const;                // 0x00036334
    bool IsReplacementSkill(SkillType skill) const;         // 0x00036304
    int GetNonBaseSkillIndex(SkillType skill) const;        // 0x000362d4
    int GetReplacementSkillCount() const;                   // 0x00036364
    int GetReplacementSkillsAllowed() const;                // 0x00035dc0
    void SetReplacementSkill(int slot, SkillType skill);    // 0x000364dc
    const std::set<int>& GetSkillPool();                    // 0x0003707c

    static const char* GetClassName(ClassType type);                    // 0x00035e94
    static const char* GetRaceName(RaceType race);                      // 0x00035ea4
    static const char* GetPerkName(PerkType perk);                      // 0x00035eb4
    static const char* GetPerkDescription(PerkType perk);               // 0x00035ed4
    static const char* GetFlawName(FlawType flaw);                      // 0x00035ec4
    static const char* GetFlawDescription(FlawType flaw);               // 0x00035ee4
    static const char* GetRacialBonusName(RaceType race);               // 0x00035ef4
    static const char* GetRacialBonusDescription(RaceType race);        // 0x00035f04
    static const char* GetMortalEnemyDescription(RaceType race);        // 0x00035f14
    static const char* GetUnlockDescription(ClassType type);            // 0x00035f24
    static EnemyType GetMortalEnemyType(RaceType race);                 // 0x00035f34
    static void GetIconCoords(int row, int col, int smallRow, GLfloat2& topLeft, GLfloat2& bottomRight);  // 0x00035c00
    static void GetIconCoords(FlawType flaw, GLfloat2& topLeft, GLfloat2& bottomRight);   // 0x00035cb8
    static void GetIconCoords(PerkType perk, GLfloat2& topLeft, GLfloat2& bottomRight);   // 0x00035cd0
    static void GetIconCoords(RaceType race, GLfloat2& topLeft, GLfloat2& bottomRight);   // 0x00035ce8
    static void GetIconCoords(ClassType type, GLfloat2& topLeft, GLfloat2& bottomRight);  // 0x00035d04
    static void GetDummyIconCoords(GLfloat2& topLeft, GLfloat2& bottomRight);              // 0x00035d20
    static void GetDummySmallIconCoords(GLfloat2& topLeft, GLfloat2& bottomRight);         // 0x00035d4c

    Game* m_game;
    ClassType m_class;
    RaceType m_race;
    RaceType m_nativeRace;
    PerkType m_perks[4];
    FlawType m_flaws[4];
    std::vector<SkillType> m_baseSkills;
    std::vector<SkillType> m_nonBaseSkills;
    std::vector<SkillType> m_replacementSkills;   // SKILL_NONE (0xff) = not replaced
    uint8_t m_level;
    int m_gamesPlayed;
    int m_enemiesKilled;
    unsigned m_bestScore[3];                      // DIFF_NORMAL, DIFF_HARD, DIFF_HARDER
    char m_levelDescriptionText[256];
    std::set<int> m_skillPool;
};
