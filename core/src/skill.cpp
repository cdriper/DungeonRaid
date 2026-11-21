#include "skill.h"

#include <cstdio>
#include <cstring>

#include "game.h"
#include "game_state.h"
#include "game_tables.h"

namespace {
// Skill types after SKILL_COUNT (the SWIG enum stops there; g_skillNames has 42 entries)
const int kSkillShock = 40;
const int kSkillAwe = 41;
const int kFirstStatSkill = 0x2a;   // m_type of a stat choice = 0x2a + StatType
}  // namespace

// The original leaves level, cooldown, charge, the texts and the bonuses uninitialised; the port
// zeroes them. m_type starts as -1.
Skill::Skill(Game* game)
    : m_game(game),
      m_active(false),
      m_type(static_cast<SkillType>(-1)),
      m_level(0),
      m_cooldown(0),
      m_charge(0),
      m_levelLabel(&game->m_fonts[0]),
      m_statsTextCount(0),
      m_statsText{},
      m_maxHealthBonus(0),
      m_healAmountBonus(0),
      m_armourAmountBonus(0),
      m_baseDamageBonus(0),
      m_coinTokenChanceBonus(0),
      m_potionTokenChanceBonus(0),
      m_skullTokenChanceBonus(0),
      m_shieldTokenChanceBonus(0) {}

Skill::~Skill() = default;

// Level number in the icon's bottom right corner: scale 0.08, right edge at 0.14375
// (x = 0.14375 - width * 0.08, an unfused vmls in the original).
void Skill::PlaceLevelLabel() {
    char text[16];   // the original prints into small static buffers (0x000896c8 / 0x000896cc)
    std::sprintf(text, "%d", m_level);
    m_levelLabel.SetText(text, -1);
    float width = m_levelLabel.m_width;
    m_levelLabel.m_scale = {0.08f, 0.08f};
    m_levelLabel.m_position.x = 0.14375f - width * 0.08f;
    m_levelLabel.m_position.y = 0.146875f;
}

// A new skill starts at level 1, ready (charge = cooldown). A level-up lowers the cooldown by 1
// (not from level 10), keeps the charge (at most the new cooldown) and adds a level (max 10).
// Sprint challenge: cooldown - 4, level 5.
void Skill::Initialise(SkillType type, Skill* previous) {
    m_type = type;
    SetIcon(g_skillIconPositions[type][0], g_skillIconPositions[type][1], 1024, 512);
    std::strcpy(m_name, g_skillNames[m_type]);
    if (previous == nullptr) {
        m_cooldown = g_skillCooldowns[type];
    } else {
        m_cooldown = previous->m_level != 10 ? previous->m_cooldown - 1 : previous->m_cooldown;
    }
    bool sprint = m_game->m_challenge && m_game->m_mode == MODE_SPRINT;
    if (sprint) m_cooldown -= 4;
    if (previous == nullptr) {
        m_charge = m_cooldown;
        m_level = 1;
    } else {
        m_charge = previous->m_charge < m_cooldown ? previous->m_charge : m_cooldown;
        m_level = previous->m_level + 1;
    }
    if (sprint) m_level = 5;
    if (m_level > 9) m_level = 10;
    PlaceLevelLabel();
    if (m_cooldown < 0) {
        m_statsText[0][0] = '\0';
    } else if (previous == nullptr) {
        std::sprintf(m_statsText[0], "Cooldown: %d", m_cooldown);
    } else {
        std::sprintf(m_statsText[0], "Cooldown: %d>>%d", previous->m_cooldown, m_cooldown);
    }
}

void Skill::Initialise(StatType stat) {
    m_type = static_cast<SkillType>(stat + kFirstStatSkill);
    SetIcon(g_statSkillIconPositions[stat][0], g_statSkillIconPositions[stat][1], 1024, 512);
    std::strcpy(m_name, g_statNames[stat]);
    m_maxHealthBonus = 0;
    m_healAmountBonus = 0;
    m_armourAmountBonus = 0;
    m_baseDamageBonus = 0;
    m_coinTokenChanceBonus = 0;
    m_potionTokenChanceBonus = 0;
    m_skullTokenChanceBonus = 0;
    m_shieldTokenChanceBonus = 0;
    m_statsTextCount = 2;
    Game* g = m_game;
    switch (stat) {
        case STAT_STR: {
            int bonus = GetStat(stat);
            m_baseDamageBonus = bonus;
            std::sprintf(m_statsText[0], "Base dmg: %d >> %d", g->m_baseDamage, bonus + g->m_baseDamage);
            SetStatChance(1, "Chance of bonus XP: ", m_skullTokenChanceBonus, g->m_baseSkullTokenChance);
            break;
        }
        case STAT_DEX: {
            int bonus = GetStat(stat);
            m_armourAmountBonus = bonus;
            std::sprintf(m_statsText[0], "Armour per shield: %d >> %d", g->m_baseArmourAmount, bonus + g->m_baseArmourAmount);
            SetStatChance(1, "Chance of bonus shield: ", m_shieldTokenChanceBonus, g->m_baseShieldTokenChance);
            break;
        }
        case STAT_VIT: {
            int bonus = GetStat(stat);
            m_maxHealthBonus = bonus;
            if (g->m_challenge && g->m_mode == MODE_PRETZEL) {
                m_statsText[0][0] = '\0';   // no max health in a Pretzel challenge
            } else {
                std::sprintf(m_statsText[0], "Max health: %d >> %d", g->m_baseMaxHealth, bonus + g->m_baseMaxHealth);
            }
            SetStatChance(1, "Chance of bonus potion: ", m_potionTokenChanceBonus, g->m_basePotionTokenChance);
            break;
        }
        case STAT_LUK: {
            int bonus = GetStat(stat);
            m_healAmountBonus = bonus;
            std::sprintf(m_statsText[0], "Health per potion: %d >> %d", g->m_baseHealAmount, bonus + g->m_baseHealAmount);
            SetStatChance(1, "Chance of bonus coin: ", m_coinTokenChanceBonus, g->m_baseCoinTokenChance);
            break;
        }
        case STAT_DMG:
            m_statsTextCount = 1;
            m_baseDamageBonus = 2;
            std::sprintf(m_statsText[0], "Base dmg: %d >> %d", g->m_baseDamage, g->m_baseDamage + 2);
            break;
        case STAT_DEF:
            m_statsTextCount = 1;
            m_armourAmountBonus = 2;
            std::sprintf(m_statsText[0], "Armour per shield: %d >> %d", g->m_baseArmourAmount, g->m_baseArmourAmount + 2);
            break;
        case STAT_HP:
            m_statsTextCount = 1;
            m_maxHealthBonus = 10;
            std::sprintf(m_statsText[0], "Max health: %d >> %d", g->m_baseMaxHealth, g->m_baseMaxHealth + 10);
            break;
        case STAT_CHA:
            m_statsTextCount = 4;
            SetStatChance(0, "Coin ", m_coinTokenChanceBonus, g->m_baseCoinTokenChance);
            SetStatChance(1, "XP ", m_skullTokenChanceBonus, g->m_baseSkullTokenChance);
            SetStatChance(2, "Shield ", m_shieldTokenChanceBonus, g->m_baseShieldTokenChance);
            SetStatChance(3, "Potion ", m_potionTokenChanceBonus, g->m_basePotionTokenChance);
            break;
        default:
            break;
    }
}

// Lasting skills set a Game flag (plus its next-turn copy from m_chainMadeThisTurn) and mark the
// skill active; the others run a board action. Every skill queues its sound first (Wish after
// its achievement check).
void Skill::Activate() {
    Game* g = m_game;
    switch (static_cast<int>(m_type)) {
        case SKILL_STEAL:
            g->QueueSound(SOUND_ABILITY);
            g->m_stealActive = true;
            g->m_stealActiveNextTurn = g->m_chainMadeThisTurn;
            m_active = true;
            break;
        case SKILL_FREEZE: g->QueueSound(SOUND_FREEZE); g->FreezeSkulls(); break;
        case SKILL_HEAL: g->QueueSound(SOUND_ABILITY); g->GrabTiles(TILE_POTION, TILE_POTION); break;
        case SKILL_REPAIR: g->QueueSound(SOUND_ABILITY); g->GrabTiles(TILE_SHIELD, TILE_SHIELD); break;
        case SKILL_COUNTER:
            g->QueueSound(SOUND_ABILITY);
            g->m_counterActive = true;
            m_active = true;
            break;
        case SKILL_SHATTER: g->QueueSound(SOUND_SHATTER); g->ShatterDefence(); break;
        case SKILL_GOLD_TOUCH: g->QueueSound(SOUND_ABILITY); g->ConvertTiles(TILE_SWORD, TILE_COIN); break;
        case SKILL_TELEPORT: g->QueueSound(SOUND_TELEPORT); g->Teleport(); break;
        case SKILL_EXPLOSIVE: g->QueueSound(SOUND_EXPLOSIVE); g->CreateExplosivePotions(); break;
        case SKILL_ELIXIR: g->QueueSound(SOUND_ABILITY); g->GrabTiles(TILE_POTION, TILE_SKULL); break;
        case SKILL_ENCHANT: g->QueueSound(SOUND_ABILITY); g->Upgrade(); break;
        case SKILL_SLASH: g->QueueSound(SOUND_SLASH); g->CollectRandomRow(); break;
        case SKILL_TREASURE: g->QueueSound(SOUND_ABILITY); g->GrabTiles(TILE_COIN, TILE_COIN); break;
        case SKILL_SCAVENGE: g->QueueSound(SOUND_ABILITY); g->ConvertTiles(TILE_SWORD, TILE_SHIELD); break;
        case SKILL_DISARM: g->QueueSound(SOUND_ABILITY); g->ConvertTiles(TILE_SKULL, TILE_SWORD); break;
        case SKILL_DAZZLE: g->QueueSound(SOUND_ABILITY); g->ConvertTiles(TILE_COIN, TILE_SWORD); break;
        case SKILL_BOOST_DMG:
            g->QueueSound(SOUND_BOOST);
            g->m_doubleDamage = true;
            g->m_doubleDamageNextTurn = g->m_chainMadeThisTurn;
            m_active = true;
            break;
        case SKILL_BOOST_GOLD:
            g->QueueSound(SOUND_BOOST);
            g->m_doubleGold = true;
            g->m_doubleGoldNextTurn = g->m_chainMadeThisTurn;
            m_active = true;
            break;
        case SKILL_BOOST_HEALTH:
            g->QueueSound(SOUND_BOOST);
            g->m_doubleHealth = true;
            g->m_doubleHealthNextTurn = g->m_chainMadeThisTurn;
            m_active = true;
            break;
        case SKILL_BOOST_ARMOUR:
            g->QueueSound(SOUND_BOOST);
            g->m_doubleArmour = true;
            g->m_doubleArmourNextTurn = g->m_chainMadeThisTurn;
            m_active = true;
            break;
        case SKILL_EXORCISE: g->QueueSound(SOUND_ABILITY); g->DemoteSpecialEnemy(); break;
        case SKILL_BANISH: g->QueueSound(SOUND_ATTACK_MISS); g->KillSpecialEnemy(); break;
        case SKILL_TRAP:
            g->QueueSound(SOUND_ATTACK_HIT);
            g->m_trapActive = true;
            g->m_trapActiveNextTurn = g->m_chainMadeThisTurn;
            m_active = true;
            break;
        case SKILL_BIG_GAME: g->QueueSound(SOUND_SKULL); g->PromoteNormalEnemy(); break;
        case SKILL_TREASURE_CHAMBER:
            g->QueueSound(SOUND_DOOR);
            g->m_treasureChamberActive = true;
            g->m_treasureChamberActiveNextTurn = g->m_chainMadeThisTurn;
            m_active = true;
            break;
        case SKILL_MAGIC_SWORD: g->QueueSound(SOUND_ABILITY); g->CreateMagicSword(); break;
        case SKILL_EARTHQUAKE: g->QueueSound(SOUND_QUAKE); g->Earthquake(); break;
        case SKILL_EXPLOSIVE_ARMOUR: g->QueueSound(SOUND_EXPLOSION); g->ExplodeArmour(); break;
        case SKILL_FIREBALL: g->QueueSound(SOUND_EXPLOSION); g->CollectRandomBlock(); break;
        case SKILL_MANA: g->QueueSound(SOUND_EXPLOSIVE); g->CreateManaPotions(); break;
        case SKILL_HACK: g->QueueSound(SOUND_ATTACK_HIT); g->ConvertColumnToSwords(); break;
        case SKILL_VOLLEY: g->QueueSound(SOUND_ATTACK_MISS); g->CreateArrows(); break;
        case SKILL_MASOCHISM: g->QueueSound(SOUND_SKULL); g->ConvertTiles(TILE_COIN, TILE_SKULL); break;
        case SKILL_WISH: {
            int skulls = g->GetTileCount(TILE_SKULL);
            int special = g->GetSpecialEnemyCount();
            if (skulls - special > 19) {
                g->QueueAchievement(ACH_WHEN_YOU_WISH, 100.0f);
            }
            g->QueueSound(SOUND_ABILITY);
            g->CollectBoard();
            g->RemoveSkill(this);   // one use only
            break;
        }
        case SKILL_BERSERK:
            g->QueueSound(SOUND_SKULL);
            g->m_berserkActive = true;
            g->m_berserkActiveNextTurn = g->m_chainMadeThisTurn;
            m_active = true;
            break;
        case SKILL_RUNE:
            g->QueueSound(SOUND_ABILITY);
            g->m_runeActive = true;
            g->m_arrow.SetColour(0x50, 0x82, 0xff);   // blue chain arrow
            g->m_runeActiveNextTurn = g->m_chainMadeThisTurn;
            m_active = true;
            break;
        case SKILL_GREED:
            g->QueueSound(SOUND_COIN);
            g->QueueSound(SOUND_COIN);
            g->QueueSound(SOUND_COIN);
            g->KillSkullsNextToCoins();
            break;
        case SKILL_LAST_STAND:
            g->QueueSound(SOUND_ABILITY);
            g->m_lastStandActive = true;
            g->m_lastStandActiveNextTurn = g->m_chainMadeThisTurn;
            m_active = true;
            break;
        case SKILL_CHARGE: g->QueueSound(SOUND_ATTACK_HIT); g->CollectEdgeTiles(); break;
        case SKILL_HASTE: g->QueueSound(SOUND_BOOST); g->CreateChain(); break;
        case kSkillShock: g->QueueSound(SOUND_EXPLOSION); g->Shock(); break;
        case kSkillAwe: g->QueueSound(SOUND_EXPLOSION); g->Awe(); break;
        default: break;
    }
}

void Skill::Deactivate() {
    m_active = false;
}

// Whether the board has something for the skill to act on.
bool Skill::CanActivate(int type) {
    Game* g = m_game;
    switch (type) {
        case SKILL_STEAL:
        case SKILL_FREEZE:
        case SKILL_COUNTER:
        case 0x22:   // SKILL_BERSERK
            return g->GetTileCount(TILE_SKULL) > 0;
        case SKILL_HEAL:
        case SKILL_EXPLOSIVE:
        case SKILL_ELIXIR:
        case SKILL_MANA:
            return g->GetNormalPotionCount() > 0;
        case SKILL_REPAIR:
            return g->GetTileCount(TILE_SHIELD) > 0;
        case SKILL_SHATTER:
        case SKILL_DISARM:
        case SKILL_BIG_GAME: {
            int skulls = g->GetTileCount(TILE_SKULL);
            return skulls - g->GetSpecialEnemyCount() > 0;
        }
        case SKILL_GOLD_TOUCH:
        case SKILL_SCAVENGE:
            return g->GetTileCount(TILE_SWORD) > 0;
        case SKILL_EXORCISE:
        case SKILL_BANISH:
            return g->GetSpecialEnemyCount() > 0;
        case SKILL_MAGIC_SWORD:
        case SKILL_VOLLEY:
            return g->GetNormalSwordCount() > 0;
        case SKILL_EXPLOSIVE_ARMOUR:
            if (g->GetTileCount(TILE_SKULL) == 0) return false;
            return g->m_armour > 0;
        case SKILL_GREED: {
            int skulls = g->GetTileCount(TILE_SKULL);
            if (skulls - g->GetSpecialEnemyCount() < 1) return false;
            return g->GetTileCount(TILE_COIN) > 0;
        }
        case SKILL_TREASURE:
        case SKILL_DAZZLE:
        case SKILL_MASOCHISM:
            return g->GetTileCount(TILE_COIN) > 0;
        default:
            return true;
    }
}

bool Skill::CanActivate() {
    return CanActivate(m_type);
}

const char* Skill::GetDescription() {
    if (static_cast<int>(m_type) < kFirstStatSkill) {
        return g_skillDescriptions[m_type];
    }
    return "";
}

char* Skill::GetStats(int line) {
    return m_statsText[line];
}

// "<label><chance>% >> <chance + bonus>%" (capped at 100), or "<label>100%" with no bonus once
// the chance is 100.
void Skill::SetStatChance(int line, const char* label, int& bonus, int chance) {
    if (chance < 100) {
        int b = GetStatChance();
        bonus = b;
        if (b > 0) {
            int next = b + chance;
            if (next > 99) next = 100;
            std::sprintf(m_statsText[line], "%s%d%% >> %d%%", label, chance, next);
            return;
        }
    } else {
        bonus = 0;
    }
    std::sprintf(m_statsText[line], "%s100%%", label);
}

// Bytes: type, cooldown, charge, level (one byte each).
void Skill::Serialise(GameState* state) {
    state->WriteChar(static_cast<uint8_t>(m_type));
    state->WriteChar(static_cast<uint8_t>(m_cooldown));
    state->WriteChar(static_cast<uint8_t>(m_charge));
    state->WriteChar(static_cast<uint8_t>(m_level));
}

// A cooldown byte of 0xff means no cooldown (cooldown and charge -1). Type bytes past the
// tables read out of bounds (undefined behaviour in the original as well).
void Skill::Deserialise(GameState* state) {
    m_type = static_cast<SkillType>(state->ReadChar());
    m_cooldown = state->ReadChar();
    m_charge = state->ReadChar();
    int level = state->ReadChar();
    if (m_cooldown == 0xff) {
        m_cooldown = -1;
        m_charge = -1;
    }
    m_level = level;
    SetIcon(m_type % 10, m_type / 10, 1024, 512);
    std::strcpy(m_name, g_skillNames[m_type]);
    PlaceLevelLabel();
}

void Skill::RenderExtras() {
    if (static_cast<int>(m_type) < kFirstStatSkill) {
        m_levelLabel.Render();
    }
}

const char* Skill::GetSkillName(SkillType type) {
    return g_skillNames[type];
}

const char* Skill::GetSkillDescription(SkillType type) {
    return g_skillDescriptions[type];
}

int Skill::GetBaseCooldown(SkillType type) {
    return g_skillCooldowns[type];
}

int Skill::GetStat(StatType stat) {
    if (static_cast<unsigned>(stat) < STAT_DMG) {
        return g_statSkillValues[stat];
    }
    return 0;
}

int Skill::GetStatChance() {
    return 5;
}

void Skill::GetIconUVs(SkillType type, GLfloat2& topLeft, GLfloat2& bottomRight) {
    Upgrade::GetIconUVs(type % 10, type / 10, 1024, 512, topLeft, bottomRight);
}
