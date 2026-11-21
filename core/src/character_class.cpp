#include "character_class.h"

#include <cstdio>
#include <cstring>

#include "game.h"
#include "game_state.h"
#include "game_tables.h"
#include "skill.h"
#include "util.h"

namespace {
// Per class (index = ClassType, CLASS_ADVENTURER handled separately): race (also the native
// race), first perk and flaw, and the 20 base skills in the original's push_back order.
struct ClassDefinition {
    RaceType race;
    PerkType perk;
    FlawType flaw;
    SkillType skills[20];
};

const ClassDefinition kClasses[CLASS_COUNT] = {
    {},   // CLASS_ADVENTURER: see Initialise
    {RACE_ELF, PERK_AGILE, FLAW_SLIM,
     {SKILL_VOLLEY, SKILL_HEAL, SKILL_REPAIR, SKILL_COUNTER, SKILL_SHATTER, SKILL_TELEPORT, SKILL_EXPLOSIVE,
      SKILL_ELIXIR, SKILL_ENCHANT, SKILL_TREASURE, SKILL_SCAVENGE, SKILL_DISARM, SKILL_BOOST_DMG,
      SKILL_BOOST_HEALTH, SKILL_BANISH, SKILL_TRAP, SKILL_BIG_GAME, SKILL_TREASURE_CHAMBER, SKILL_MAGIC_SWORD,
      SKILL_MANA}},
    {RACE_HALFLING, PERK_DARING, FLAW_COCKY,
     {SKILL_MASOCHISM, SKILL_HEAL, SKILL_REPAIR, SKILL_COUNTER, SKILL_GOLD_TOUCH, SKILL_EXPLOSIVE, SKILL_ELIXIR,
      SKILL_SLASH, SKILL_TREASURE, SKILL_DAZZLE, SKILL_BOOST_DMG, SKILL_BOOST_GOLD, SKILL_BOOST_HEALTH,
      SKILL_BOOST_ARMOUR, SKILL_EXORCISE, SKILL_TRAP, SKILL_BIG_GAME, SKILL_TREASURE_CHAMBER, SKILL_MAGIC_SWORD,
      SKILL_EXPLOSIVE_ARMOUR}},
    {RACE_FAUN, PERK_FAITHFUL, FLAW_CAUTIOUS,
     {SKILL_WISH, SKILL_FREEZE, SKILL_HEAL, SKILL_SHATTER, SKILL_GOLD_TOUCH, SKILL_TELEPORT, SKILL_EXPLOSIVE,
      SKILL_ELIXIR, SKILL_ENCHANT, SKILL_TREASURE, SKILL_SCAVENGE, SKILL_DAZZLE, SKILL_BOOST_HEALTH,
      SKILL_BOOST_ARMOUR, SKILL_BANISH, SKILL_EXORCISE, SKILL_MAGIC_SWORD, SKILL_EARTHQUAKE, SKILL_FIREBALL,
      SKILL_MANA}},
    {RACE_ORC, PERK_STRONG, FLAW_RECKLESS,
     {SKILL_BERSERK, SKILL_REPAIR, SKILL_COUNTER, SKILL_SHATTER, SKILL_TELEPORT, SKILL_ELIXIR, SKILL_ENCHANT,
      SKILL_SLASH, SKILL_TREASURE, SKILL_SCAVENGE, SKILL_DISARM, SKILL_BOOST_DMG, SKILL_BOOST_HEALTH,
      SKILL_BOOST_ARMOUR, SKILL_BANISH, SKILL_EXORCISE, SKILL_TRAP, SKILL_BIG_GAME, SKILL_MAGIC_SWORD,
      SKILL_EXPLOSIVE_ARMOUR}},
    {RACE_CHANGELING, PERK_GIFTED, FLAW_FRAIL,
     {SKILL_RUNE, SKILL_FREEZE, SKILL_HEAL, SKILL_REPAIR, SKILL_GOLD_TOUCH, SKILL_TELEPORT, SKILL_EXPLOSIVE,
      SKILL_ELIXIR, SKILL_ENCHANT, SKILL_TREASURE, SKILL_DAZZLE, SKILL_BOOST_GOLD, SKILL_BOOST_HEALTH,
      SKILL_BANISH, SKILL_EXORCISE, SKILL_TREASURE_CHAMBER, SKILL_MAGIC_SWORD, SKILL_EARTHQUAKE, SKILL_FIREBALL,
      SKILL_MANA}},
    {RACE_SAUREN, PERK_FRUGAL, FLAW_CLUMSY,
     {SKILL_GREED, SKILL_STEAL, SKILL_HEAL, SKILL_REPAIR, SKILL_COUNTER, SKILL_GOLD_TOUCH, SKILL_EXPLOSIVE,
      SKILL_ELIXIR, SKILL_ENCHANT, SKILL_SLASH, SKILL_TREASURE, SKILL_SCAVENGE, SKILL_DISARM, SKILL_DAZZLE,
      SKILL_BOOST_GOLD, SKILL_BOOST_HEALTH, SKILL_TRAP, SKILL_BIG_GAME, SKILL_TREASURE_CHAMBER,
      SKILL_MAGIC_SWORD}},
    {RACE_DRAGONKIN, PERK_INTIMIDATING, FLAW_ZEALOUS,
     {SKILL_LAST_STAND, SKILL_HEAL, SKILL_REPAIR, SKILL_COUNTER, SKILL_SHATTER, SKILL_EXPLOSIVE, SKILL_ELIXIR,
      SKILL_SLASH, SKILL_TREASURE, SKILL_SCAVENGE, SKILL_DISARM, SKILL_BOOST_DMG, SKILL_BOOST_HEALTH,
      SKILL_BOOST_ARMOUR, SKILL_BANISH, SKILL_EXORCISE, SKILL_BIG_GAME, SKILL_TREASURE_CHAMBER,
      SKILL_MAGIC_SWORD, SKILL_EXPLOSIVE_ARMOUR}},
    {RACE_DWARF, PERK_TOUGH, FLAW_NOISY,
     {SKILL_CHARGE, SKILL_HEAL, SKILL_REPAIR, SKILL_COUNTER, SKILL_SHATTER, SKILL_GOLD_TOUCH, SKILL_TELEPORT,
      SKILL_ENCHANT, SKILL_SLASH, SKILL_TREASURE, SKILL_SCAVENGE, SKILL_DISARM, SKILL_DAZZLE, SKILL_BOOST_DMG,
      SKILL_BOOST_ARMOUR, SKILL_BANISH, SKILL_TRAP, SKILL_BIG_GAME, SKILL_MAGIC_SWORD, SKILL_EXPLOSIVE_ARMOUR}},
    {RACE_WEREWOLF, PERK_CUNNING, FLAW_SNEAKY,
     {SKILL_HASTE, SKILL_STEAL, SKILL_FREEZE, SKILL_COUNTER, SKILL_SHATTER, SKILL_TELEPORT, SKILL_EXPLOSIVE,
      SKILL_ELIXIR, SKILL_TREASURE, SKILL_DISARM, SKILL_DAZZLE, SKILL_BOOST_DMG, SKILL_BOOST_HEALTH,
      SKILL_BANISH, SKILL_TRAP, SKILL_BIG_GAME, SKILL_TREASURE_CHAMBER, SKILL_MAGIC_SWORD,
      SKILL_EXPLOSIVE_ARMOUR, SKILL_FIREBALL}},
};

// Skill types 0..41 exist (the SWIG enum stops at 40, the skill tables have 42 entries).
const int kSkillTypes = 42;
const int kReplacementSlots = 20;   // fixed count in (De)SerialiseGameState, Validate, GetSkillPool
}  // namespace

// The original only initialises the vectors, the five statistics and the skill-pool set; the
// port zeroes the rest too.
CharacterClass::CharacterClass()
    : m_game(nullptr),
      m_class(CLASS_ADVENTURER),
      m_race(RACE_HUMAN),
      m_nativeRace(RACE_HUMAN),
      m_perks{PERK_UNEXPECTED, PERK_UNEXPECTED, PERK_UNEXPECTED, PERK_UNEXPECTED},
      m_flaws{FLAW_CONFUSED, FLAW_CONFUSED, FLAW_CONFUSED, FLAW_CONFUSED},
      m_level(0),
      m_gamesPlayed(0),
      m_enemiesKilled(0),
      m_bestScore{0, 0, 0},
      m_levelDescriptionText{} {}

CharacterClass::~CharacterClass() = default;

// Note: appends to the skill vectors without clearing them (as the original).
void CharacterClass::Initialise(ClassType type, Game* game) {
    m_level = 0;
    m_game = game;
    m_class = type;
    for (int i = 0; i < 4; ++i) {
        m_perks[i] = PERK_NONE;
        m_flaws[i] = FLAW_NONE;
    }
    if (type == CLASS_ADVENTURER) {
        m_level = 1;
        m_race = RACE_HUMAN;
        m_nativeRace = RACE_HUMAN;
        for (int s = 0; s < 20; ++s) {
            m_baseSkills.push_back(static_cast<SkillType>(s));
        }
    } else if (static_cast<unsigned>(type) < CLASS_COUNT) {
        const ClassDefinition& def = kClasses[type];
        m_race = def.race;
        m_nativeRace = def.race;
        m_perks[0] = def.perk;
        m_flaws[0] = def.flaw;
        for (SkillType s : def.skills) {
            m_baseSkills.push_back(s);
        }
    }
    for (int s = 0; s < kSkillTypes; ++s) {
        if (!IsBaseSkill(static_cast<SkillType>(s))) {
            m_nonBaseSkills.push_back(static_cast<SkillType>(s));
        }
    }
    for (size_t i = 0; i < m_baseSkills.size(); ++i) {
        m_replacementSkills.push_back(SKILL_NONE);
    }
}

// race, perks/flaws 1..3, replacement slots 1..19, one byte each
void CharacterClass::SerialiseGameState(GameState* state) {
    state->WriteChar(static_cast<uint8_t>(m_race));
    for (int i = 1; i < 4; ++i) {
        state->WriteChar(static_cast<uint8_t>(m_perks[i]));
        state->WriteChar(static_cast<uint8_t>(m_flaws[i]));
    }
    for (int i = 1; i < kReplacementSlots; ++i) {
        state->WriteChar(static_cast<uint8_t>(m_replacementSkills[i]));
    }
}

void CharacterClass::DeserialiseGameState(GameState* state) {
    m_race = static_cast<RaceType>(state->ReadChar());
    for (int i = 1; i < 4; ++i) {
        m_perks[i] = static_cast<PerkType>(state->ReadChar());
        m_flaws[i] = static_cast<FlawType>(state->ReadChar());
    }
    for (int i = 1; i < kReplacementSlots; ++i) {
        m_replacementSkills[i] = static_cast<SkillType>(state->ReadChar());
    }
}

void CharacterClass::SerialiseFullState(GameState* state) {
    Validate();
    SerialiseGameState(state);
    state->WriteChar(m_level);
    state->WriteInteger(m_gamesPlayed);
    state->WriteInteger(m_enemiesKilled);
    for (unsigned score : m_bestScore) {
        state->WriteInteger(static_cast<int>(score));
    }
}

void CharacterClass::DeserialiseFullState(GameState* state) {
    DeserialiseGameState(state);
    m_level = static_cast<uint8_t>(ClampI(state->ReadChar(), 0, 10));
    if (m_class == CLASS_ADVENTURER) {
        m_level = 1;
    }
    m_gamesPlayed = state->ReadInteger();
    m_enemiesKilled = state->ReadInteger();
    for (unsigned& score : m_bestScore) {
        score = static_cast<unsigned>(state->ReadInteger());
    }
    Validate();
}

void CharacterClass::Validate() {
    // duplicate perks / flaws: clear the later copy (values change while iterating, as in the original)
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (i != j && m_perks[i] == m_perks[j]) {
                m_perks[j] = PERK_NONE;
            }
        }
    }
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (i != j && m_flaws[i] == m_flaws[j]) {
                m_flaws[j] = FLAW_NONE;
            }
        }
    }
    // a perk needs the flaw in the same slot
    for (int i = 0; i < 4; ++i) {
        if (m_perks[i] != PERK_NONE && m_flaws[i] == FLAW_NONE) {
            m_perks[i] = PERK_NONE;
        }
    }
    // Meant to drop replacements beyond the allowed number, but the original starts from
    // GetReplacementSkillCount() (not ...Allowed()), so the counter never goes below zero and
    // nothing is dropped. Kept as is.
    int remaining = GetReplacementSkillCount();
    for (int i = 0; i < kReplacementSlots; ++i) {
        if (m_replacementSkills[i] != SKILL_NONE) {
            --remaining;
        }
        if (remaining < 0) {
            m_replacementSkills[i] = SKILL_NONE;
        }
    }
    if (m_class != CLASS_ADVENTURER && m_level <= 4) {
        m_race = m_nativeRace;
    }
}

bool CharacterClass::IsPerkUnlocked(int slot) const {
    switch (slot) {
        case 0: return m_level != 0;
        case 1: return m_level > 2;
        case 2: return m_level > 6;
        case 3: return m_level >= 9;
        default: return false;
    }
}

bool CharacterClass::HavePerk(PerkType perk) const {
    if (perk == PERK_NONE) return false;
    return m_perks[0] == perk || m_perks[1] == perk || m_perks[2] == perk || m_perks[3] == perk;
}

bool CharacterClass::HaveFlaw(FlawType flaw) const {
    if (flaw == FLAW_NONE) return false;
    return m_flaws[0] == flaw || m_flaws[1] == flaw || m_flaws[2] == flaw || m_flaws[3] == flaw;
}

void CharacterClass::SetPerk(int slot, PerkType perk) {
    if (slot > 0 && IsPerkUnlocked(slot) && !HavePerk(perk)) {
        m_perks[slot] = perk;
    }
}

void CharacterClass::SetFlaw(int slot, FlawType flaw) {
    if (slot > 0 && IsPerkUnlocked(slot) && !HaveFlaw(flaw)) {
        m_flaws[slot] = flaw;
    }
}

int CharacterClass::GetPerkCount() const {
    if (m_class == CLASS_ADVENTURER) return 0;
    int n = 0;
    for (PerkType p : m_perks) n += p != PERK_NONE;
    return n;
}

int CharacterClass::GetFlawCount() const {
    if (m_class == CLASS_ADVENTURER) return 0;
    int n = 0;
    for (FlawType f : m_flaws) n += f != FLAW_NONE;
    return n;
}

void CharacterClass::SetLevel(int level) {
    m_level = static_cast<uint8_t>(ClampI(level, 0, 10));
}

void CharacterClass::LevelUp() {
    m_level = static_cast<uint8_t>(ClampI(m_level + 1, 1, 10));
    UnlockAchievement();
}

// Achievement ids: the class (ACH_RANGER == CLASS_RANGER, ...), native race + ACH_ASSASSIN
// (ACH_ELF == 10 for RACE_ELF), class + ACH_CHANGELING (master achievements).
void CharacterClass::UnlockAchievement() {
    if (m_class == CLASS_ADVENTURER) {   // == ACH_NONE
        return;
    }
    if (m_level != 0) {
        m_game->QueueAchievement(static_cast<Achievement>(m_class), 100.0f);
        if (m_level > 4) {
            m_game->QueueAchievement(static_cast<Achievement>(m_nativeRace + ACH_ASSASSIN), 100.0f);
            if (m_level == 10) {
                m_game->QueueAchievement(static_cast<Achievement>(m_class + ACH_CHANGELING), 100.0f);
            }
        }
    }
    m_game->CheckIfAllClassesMaxed();
}

// Levels outside 1..10 leave the previous text in the buffer.
const char* CharacterClass::GetLevelDescription(int level) {
    char* out = m_levelDescriptionText;
    if (m_class == CLASS_ADVENTURER) {
        std::sprintf(out, "Unlocked perk %s and flaw %s for use with other classes", GetPerkName(PERK_UNEXPECTED),
                     GetFlawName(FLAW_CONFUSED));
        return out;
    }
    switch (level) {
        case 1:
            std::sprintf(out, "Unlock perk %s and flaw %s for use with other classes", GetPerkName(m_perks[0]),
                         GetFlawName(m_flaws[0]));
            break;
        case 2:
            std::strcpy(out, "Unlock the option to replace 2 skills in the skill list");
            break;
        case 3:
        case 7:
        case 9:
            std::strcpy(out, "Unlock one perk & flaw slot which can be filled with any unlocked perk & flaw");
            break;
        case 4:
            std::strcpy(out, "Unlock the option to replace 2 skills in the skill list (4 total)");
            break;
        case 5:
            std::sprintf(out, "Unlock the option to change race, and can use the %s race with other classes",
                         GetRaceName(m_nativeRace));
            break;
        case 6:
            std::strcpy(out, "Unlock the option to replace 2 skills in the skill list (6 total)");
            break;
        case 8:
            std::strcpy(out, "Unlock the option to replace 2 skills in the skill list (8 total)");
            break;
        case 10:
            std::sprintf(out,
                         "Unlock class skill \"%s\" and the option to replace 2 skills in the skill list (10 total)",
                         Skill::GetSkillName(m_baseSkills[0]));
            break;
        default:
            break;
    }
    return out;
}

bool CharacterClass::IsBaseSkill(SkillType skill) const {
    for (SkillType s : m_baseSkills) {
        if (s == skill) return true;
    }
    return false;
}

bool CharacterClass::IsReplacementSkill(SkillType skill) const {
    for (SkillType s : m_replacementSkills) {
        if (s == skill) return true;
    }
    return false;
}

int CharacterClass::GetNonBaseSkillIndex(SkillType skill) const {
    for (size_t i = 0; i < m_nonBaseSkills.size(); ++i) {
        if (m_nonBaseSkills[i] == skill) return static_cast<int>(i);
    }
    return -1;
}

int CharacterClass::GetReplacementSkillCount() const {
    int n = 0;
    for (SkillType s : m_replacementSkills) n += s != SKILL_NONE;
    return n;
}

// 0 below level 2, then 2 more every two levels: 2, 4, 6, 8; 10 at level 10.
int CharacterClass::GetReplacementSkillsAllowed() const {
    if (m_level < 2) return 0;
    if (m_level < 4) return 2;
    if (m_level < 6) return 4;
    if (m_level < 8) return 6;
    if (m_level != 10) return 8;
    return m_level;
}

void CharacterClass::SetReplacementSkill(int slot, SkillType skill) {
    if (slot <= 0 || static_cast<unsigned>(slot) >= m_replacementSkills.size()) {
        return;
    }
    if (m_replacementSkills[slot] == SKILL_NONE) {
        if (GetReplacementSkillCount() < GetReplacementSkillsAllowed()) {
            m_replacementSkills[slot] = skill;
        }
    } else {
        m_replacementSkills[slot] = skill;
    }
}

// Adventurer: every unlocked skill (asks the Game); others: each base skill or its replacement.
const std::set<int>& CharacterClass::GetSkillPool() {
    m_skillPool.clear();
    if (m_class == CLASS_ADVENTURER) {
        for (int s = 0; s < kSkillTypes; ++s) {
            if (m_game->IsSkillUnlocked(static_cast<SkillType>(s))) {
                m_skillPool.insert(s);
            }
        }
    } else {
        for (int i = 0; i < kReplacementSlots; ++i) {
            SkillType r = m_replacementSkills[i];
            m_skillPool.insert(r == SKILL_NONE ? m_baseSkills[i] : r);
        }
    }
    return m_skillPool;
}

const char* CharacterClass::GetClassName(ClassType type) { return g_classNames[type]; }
const char* CharacterClass::GetRaceName(RaceType race) { return g_raceNames[race]; }
const char* CharacterClass::GetPerkName(PerkType perk) { return g_perkNames[perk]; }
const char* CharacterClass::GetPerkDescription(PerkType perk) { return g_perkDescriptions[perk]; }
const char* CharacterClass::GetFlawName(FlawType flaw) { return g_flawNames[flaw]; }
const char* CharacterClass::GetFlawDescription(FlawType flaw) { return g_flawDescriptions[flaw]; }
const char* CharacterClass::GetRacialBonusName(RaceType race) { return g_racialBonusNames[race]; }
const char* CharacterClass::GetRacialBonusDescription(RaceType race) { return g_racialBonusDescriptions[race]; }
const char* CharacterClass::GetMortalEnemyDescription(RaceType race) { return g_mortalEnemyDescriptions[race]; }
const char* CharacterClass::GetUnlockDescription(ClassType type) { return g_classUnlockDescriptions[type]; }

EnemyType CharacterClass::GetMortalEnemyType(RaceType race) {
    return static_cast<unsigned>(race) < RACE_COUNT ? static_cast<EnemyType>(g_mortalEnemyTypes[race]) : ENEMY_NORMAL;
}

// Icon cells in the menus texture: big icons 51 px (row offset 70), small ones 24 px (offset 172).
void CharacterClass::GetIconCoords(int row, int col, int smallRow, GLfloat2& topLeft, GLfloat2& bottomRight) {
    if (smallRow < 0) {
        topLeft = {static_cast<float>(col * 51), static_cast<float>(row * 51 + 70)};
        bottomRight = {static_cast<float>((col + 1) * 51), static_cast<float>((row + 1) * 51 + 70)};
    } else {
        topLeft = {static_cast<float>(col * 24), static_cast<float>(smallRow * 24 + 172)};
        bottomRight = {static_cast<float>((col + 1) * 24), static_cast<float>((smallRow + 1) * 24 + 172)};
    }
}

void CharacterClass::GetIconCoords(FlawType flaw, GLfloat2& tl, GLfloat2& br) { GetIconCoords(0, flaw + FLAW_COUNT, 0, tl, br); }
void CharacterClass::GetIconCoords(PerkType perk, GLfloat2& tl, GLfloat2& br) { GetIconCoords(0, perk, 0, tl, br); }
void CharacterClass::GetIconCoords(RaceType race, GLfloat2& tl, GLfloat2& br) { GetIconCoords(1, race, -1, tl, br); }
void CharacterClass::GetIconCoords(ClassType type, GLfloat2& tl, GLfloat2& br) { GetIconCoords(0, type, -1, tl, br); }

void CharacterClass::GetDummyIconCoords(GLfloat2& topLeft, GLfloat2& bottomRight) {
    topLeft = {-51.0f, 0.0f};
    bottomRight = {0.0f, 51.0f};
}

void CharacterClass::GetDummySmallIconCoords(GLfloat2& topLeft, GLfloat2& bottomRight) {
    topLeft = {-24.0f, 0.0f};
    bottomRight = {0.0f, 24.0f};
}
