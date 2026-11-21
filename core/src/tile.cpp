#include "tile.h"

#include <cmath>
#include <cstdio>

#include "dr_gl.h"
#include "font.h"
#include "game.h"
#include "game_state.h"
#include "game_tables.h"
#include "label.h"
#include "rand.h"
#include "util.h"

// Static initialiser of the original (computed at load time; the port has them constant)
const GLfloat2 g_leftEyePositions[8] = {
    {-0.05625f, 0.009375f}, {-0.05625f, 0.00625f}, {-0.05625f, 0.00625f}, {-0.05625f, 0.009375f},
    {-0.05625f, 0.009375f}, {-0.053125f, 0.00625f}, {-0.053125f, 0.0125f}, {-0.05f, 0.00625f},
};
const GLfloat2 g_rightEyePositions[8] = {
    {0.021875f, 0.00625f}, {0.021875f, 0.009375f}, {0.021875f, 0.009375f}, {0.021875f, 0.003125f},
    {0.01875f, 0.009375f}, {0.021875f, 0.00625f}, {0.021875f, 0.003125f}, {0.021875f, 0.00625f},
};

namespace {
const float kCell = 0.0517578125f;   // 53/1024: one tile cell in the tile texture

const GLcolour4 kWhite = {0xff, 0xff, 0xff, 0xff};

const float kPopDuration = 0.15f;    // port: the selection pop (mouse only)

// The cell (variant, type) of the tile texture plus a row offset (special potions)
void SetCellUV(Quad& quad, int variant, int type, float rowOffset, bool withOffset) {
    GLfloat2 tl;
    tl.x = static_cast<float>(variant) * kCell;
    tl.y = withOffset ? static_cast<float>(type) * kCell + rowOffset : static_cast<float>(type) * kCell;
    GLfloat2 br = {tl.x + kCell, tl.y + kCell};
    quad.SetUV(tl, br);
}

bool IsUndeadLich(const Tile* t) {
    return t->m_type == TILE_SKULL && t->m_enemyType == ENEMY_LICH && t->m_countdown != 0;
}

CharacterClass& CurrentClass(Game* game) {
    return game->m_classes[game->m_currentClass];
}
}  // namespace

// The original leaves the remaining fields (position in the grid, stats, the countdown label
// pointer, ...) uninitialised; the port zeroes them.
Tile::Tile()
    : m_game(nullptr),
      m_column(0),
      m_row(0),
      m_baseSize{0.0f, 0.0f},
      m_matched(false),
      m_selected(false),
      m_spawnedThisTurn(false),
      m_targetPosition{0.0f, 0.0f},
      m_falling(false),
      m_fallSpeed(1.0f),
      m_fallAcceleration(0.5f),
      m_type(TILE_POTION),
      m_specialType(SPECIAL_TILE_CHAIN),
      m_attack(0),
      m_armour(0),
      m_health(0),
      m_maxHealth(0),
      m_renderOffsetX(0.0f),
      m_font(nullptr),
      m_attackLabel(nullptr),
      m_armourLabel(nullptr),
      m_healthLabel(nullptr),
      m_countdownLabel(nullptr),
      m_crossVisible(false),
      m_overlayVisible(false),
      m_mortalGlowVisible(false),
      m_mortalGlowSpin(0.0f),
      m_variant(0),
      m_magicSword(false),
      m_arrow(false),
      m_manaPotion(false),
      m_acidPotion(false),
      m_explosivePotion(false),
      m_poison{0, 0, 0},
      m_poisonFadeActive(false),
      m_poisonFadeOut(false),
      m_poisonFade(0.0f),
      m_enemyType(ENEMY_NORMAL),
      m_countdown(0),
      m_abilityUsed(false),
      m_frozen(false),
      m_burning(false),
      m_burnIsFireBreath(false),
      m_burnDamage(0),
      m_broken(false),
      m_promotedByBigGame(false),
      m_lichJustDied(false) {
    m_cross.SetUV(GLfloat2{0.1572265625f, 0.3232421875f}, GLfloat2{0.203125f, 0.3720703125f});
    m_cross.SetSize(GLfloat2{0.146875f, 0.15625f});
    m_mortalGlow.SetUV(GLfloat2{0.0009765625f, 0.904296875f}, GLfloat2{0.052734375f, 0.9609375f});
    m_mortalGlow.SetSize(GLfloat2{0.24843751f, 0.26718748f});
}

Tile::~Tile() = default;

// Four labels (attack, armour, health at scale 0.08, countdown at 0.13) stacked from the font's
// line metrics.
void Tile::InitialiseLabels(Game* game) {
    if (m_game != nullptr) return;
    m_game = game;
    m_font = &game->m_fonts[0];
    m_attackLabel = new Label(m_font);
    m_armourLabel = new Label(m_font);
    m_healthLabel = new Label(m_font);
    m_countdownLabel = new Label(m_font);
    m_attackLabel->m_scale = {0.08f, 0.08f};
    m_armourLabel->m_scale = {0.08f, 0.08f};
    m_healthLabel->m_scale = {0.08f, 0.08f};
    m_countdownLabel->m_scale = {0.13f, 0.13f};
    m_armourLabel->m_colour = {0xad, 0xb1, 0xff, 0xff};
    m_armourLabel->UpdateColour();
    m_healthLabel->m_colour = {0xff, 0x46, 0x3c, 0xff};
    m_healthLabel->UpdateColour();
    int lineHeight = m_font->GetLineHeight();
    float ratio = static_cast<float>(lineHeight - m_font->GetBase()) / static_cast<float>(lineHeight);
    float step = ratio * 0.08f;
    float attackY = step - 0.0828125f;
    m_attackLabel->m_position.y = attackY;
    m_armourLabel->m_position.y = step + attackY;
    m_healthLabel->m_position.y = 0.0828125f + (ratio / 2.6f) * -0.08f;
    m_countdownLabel->m_position.x = 0.0f;
    m_countdownLabel->m_position.y = attackY + ratio * 0.05333333f;
}

void Tile::DeleteLabels() {
    delete m_attackLabel;
    delete m_armourLabel;
    delete m_healthLabel;
    delete m_countdownLabel;
}

// Resets everything type-specific; skulls get stats (or are promoted when the Game says so).
void Tile::SetType(TileType type, int difficultyBase, int difficulty) {
    m_type = type;
    m_enemyType = ENEMY_NORMAL;
    m_specialType = SPECIAL_TILE_NONE;
    int variant = RandI(0, 7, RAND_COSMETIC);
    m_magicSword = false;
    m_arrow = false;
    m_manaPotion = false;
    m_acidPotion = false;
    m_explosivePotion = false;
    m_frozen = false;
    m_burning = false;
    m_burnIsFireBreath = false;
    m_broken = false;
    m_selected = false;
    m_countdown = 0;
    m_mortalGlowVisible = false;
    m_promotedByBigGame = false;
    m_lichJustDied = false;
    m_variant = variant;
    SetColour(kWhite);
    SetCellUV(*this, m_variant, m_type, 0.0f, false);
    m_overlayVisible = false;
    m_poisonFadeActive = false;
    m_poisonFadeOut = false;
    if (m_type == TILE_SKULL) {
        m_poison[0] = m_poison[1] = m_poison[2] = 0;
        if (m_game->IsTimeToPromote()) {
            Promote(difficultyBase, difficulty, false);
        } else {
            GenerateStats(difficultyBase, difficulty);
            UpdateValues();
        }
    }
}

void Tile::SetSpecialType(SpecialTileType type) {
    SetType(TILE_SPECIAL, 0, 0);
    m_specialType = type;
    if (type == SPECIAL_TILE_CHAIN) {
        SetUV(GLfloat2{0.5f, 0.4482421875f}, GLfloat2{0.5517578125f, 0.5f});
    } else if (type == SPECIAL_TILE_TROPHY) {
        SetUV(GLfloat2{0.603515625f, 0.4482421875f}, GLfloat2{0.6552734375f, 0.5f});
    }
}

// Enemy stats from the difficulty base (the turn) and difficulty: the scale grows past 500 and,
// past experience level 40, with the level; Pretzel challenges always use the Hard formulas and
// Sprint challenges their own. Also sets the number of turns until the next promotion (special
// enemies), then the special enemy's multipliers.
void Tile::GenerateStats(int difficultyBase, int difficulty) {
    Game* g = m_game;
    int d = difficultyBase;
    int xp = g->m_xp + g->m_xpBanked;
    bool challenge = g->m_challenge;
    int level;
    int promotion;
    bool pretzel = false;
    if (challenge) {
        if (g->m_mode == MODE_SPRINT) {
            level = (xp / 4) / 100;
        } else {
            level = xp / 100;
            pretzel = g->m_mode == MODE_PRETZEL;
        }
    } else {
        level = xp / 100;
    }
    // scale d by (d - 500) / divisor + 1 (at least 1) and, past level 40, by (level - 40) / 60 + 1
    auto scale = [&](float divisor) {
        float f = static_cast<float>(d - 500) / divisor + 1.0f;
        if (f < 1.0f) f = 1.0f;
        d = FloatToIntArm(static_cast<float>(d) * f);
        if (level > 40) {
            float lf = static_cast<float>(level - 40) / 60.0f + 1.0f;
            d = FloatToIntArm(static_cast<float>(d) * lf);
        }
    };
    auto promotionTurns = [&](float numerator) {
        float s = std::sqrt(static_cast<float>(d));
        return RandI(FloatToIntArm(numerator / s), FloatToIntArm(300.0f / s), RAND_TILE);
    };
    int mode = pretzel ? 1 : difficulty;
    switch (mode) {
        case 0: {
            int attack = RandI(d / 80, d / 70, RAND_TILE);
            m_attack = attack < 1 ? 1 : attack;
            int armour = RandI(d / 70, d / 55, RAND_TILE);
            m_armour = armour < 0 ? 0 : armour;
            int health = RandI(d / 25, d / 20, RAND_TILE);
            m_health = health < 3 ? 3 : health;
            promotion = promotionTurns(200.0f) + 5;
            break;
        }
        case 1: {
            scale(700.0f);
            int attack = RandI(d / 65, d / 55, RAND_TILE);
            m_attack = attack < 1 ? 1 : attack;
            int armour = RandI(d / 50, d / 40, RAND_TILE);
            m_armour = armour < 0 ? 0 : armour;
            int health = RandI(d / 20, d / 15, RAND_TILE);
            m_health = health < 4 ? 4 : health;
            promotion = promotionTurns(200.0f);
            break;
        }
        case 2: {
            scale(750.0f);
            int attack = RandI(d / 50, d / 40, RAND_TILE);
            m_attack = attack < 1 ? 1 : attack;
            int armour = RandI(d / 40, d / 37, RAND_TILE);
            m_armour = armour < 0 ? 0 : armour;
            int health = RandI(d / 17, d / 12, RAND_TILE);
            m_health = health < 4 ? 4 : health;
            promotion = promotionTurns(150.0f);
            break;
        }
        case 3: {
            scale(500.0f);
            int attack = RandI(d / 40, d / 35, RAND_TILE);
            m_attack = attack < 1 ? 1 : attack;
            int armour = RandI(d / 30, d / 35, RAND_TILE);
            m_armour = armour < 0 ? 0 : armour;
            int health = RandI(d / 15, d / 10, RAND_TILE);
            m_health = health < 4 ? 4 : health;
            promotion = promotionTurns(150.0f);
            break;
        }
        default:   // unknown difficulty: the stats stay as they were
            promotion = 1;
            break;
    }
    if (g->m_challenge && g->m_mode == MODE_SPRINT) {
        int attack = RandI(d / 20, d / 12, RAND_TILE);
        m_attack = attack < 1 ? 1 : attack;
        int armour = RandI(d / 10, d / 6, RAND_TILE);
        m_armour = armour < 0 ? 0 : armour;
        int health = RandI(d / 8, d / 4, RAND_TILE);
        m_health = health < 4 ? 4 : health;
    }
    if (m_enemyType == ENEMY_NORMAL) {
        if (CurrentClass(m_game).HaveFlaw(FLAW_SNEAKY)) m_health += 1;
    } else {
        if (CurrentClass(m_game).HaveFlaw(FLAW_ZEALOUS)) {
            promotion -= 1;
            if (promotion < 1) promotion = 1;
        }
        m_game->SetTimeToPromotion(promotion);
        if (m_armour == 0) m_armour = 1;
    }
    switch (m_enemyType) {
        case ENEMY_NORMAL: break;
        case ENEMY_BOSS: m_attack *= 3; m_armour *= 3; m_health *= 6; break;
        case ENEMY_SPIKY: m_attack *= 2; m_health *= 3; break;
        case ENEMY_GOLDEN: m_attack = 0; m_armour = m_armour * 3 * 2; m_health *= 6; m_countdown = 5; break;
        case ENEMY_INVISIBLE: m_attack *= 2; m_armour *= 10; m_health /= 2; break;
        case ENEMY_POISONOUS: m_armour *= 2; m_health *= 4; break;
        case ENEMY_KAMIKAZE: m_armour *= 2; m_health *= 6; m_countdown = 5; break;
        case ENEMY_ASSASSIN: m_attack *= 3; m_armour *= 2; m_health *= 3; break;
        case ENEMY_DOPPELGANGER: m_attack *= 2; m_armour = m_game->m_armour; m_health *= 4; break;
        case ENEMY_UNSTABLE: m_attack *= 4; m_health *= 5; m_countdown = 5; break;
        case ENEMY_ARMOURED: m_attack *= 2; m_armour *= 4; m_health *= 3; break;
        case ENEMY_DEVOURER: m_attack *= 2; m_armour *= 2; m_health *= 4; m_countdown = 5; break;
        case ENEMY_MEAT_SHIELD: m_attack /= 2; m_health *= 5; break;
        default: m_attack *= 2; m_armour *= 2; m_health *= 4; break;
    }
    if (m_enemyType != ENEMY_NORMAL && CurrentClass(m_game).HaveFlaw(FLAW_NOISY)) {
        m_health = FloatToIntArm(static_cast<float>(m_health) * 1.05f);
    }
    // mortal enemy Invisible: armour doubles while an invisible enemy is on the board
    if (CharacterClass::GetMortalEnemyType(CurrentClass(m_game).m_race) == ENEMY_INVISIBLE &&
        m_game->GetEnemyCount(ENEMY_INVISIBLE) != 0) {
        m_armour *= 2;
    }
    UpdateCountdown();
    m_maxHealth = m_health;
}

// A random special enemy (5% / 10% with Intimidating: the class's mortal enemy; never for the
// Adventurer), with stats.
void Tile::Promote(int difficultyBase, int difficulty, bool byBigGame) {
    m_enemyType = static_cast<EnemyType>(RandI(1, 32, RAND_TILE));
    CharacterClass& cls = CurrentClass(m_game);
    if (cls.m_class != CLASS_ADVENTURER) {
        int chance = cls.HavePerk(PERK_INTIMIDATING) ? 5 : 10;
        if (RandI(1, 200, RAND_TILE) <= chance) {
            m_enemyType = CharacterClass::GetMortalEnemyType(CurrentClass(m_game).m_race);
        }
    }
    GenerateStats(difficultyBase, difficulty);
    UpdateEnemyUV();
    UpdateValues();
    if (m_game->GetSpecialEnemyCount() > 2) {
        m_game->QueueAchievement(ACH_GETTING_CROWDED, 100.0f);
    }
    m_promotedByBigGame = byBigGame;
}

void Tile::UpdateEnemyUV() {
    if (m_enemyType == ENEMY_NORMAL) {
        SetCellUV(*this, m_variant, m_type, 0.0f, false);
    } else if (m_enemyType == ENEMY_LICH && m_countdown != 0) {
        SetUV(GLfloat2{0.6552734375f, 0.4482421875f}, GLfloat2{0.70703125f, 0.5f});   // dead lich
    } else {
        GLfloat2 tl, br;
        GetEnemyUV(m_enemyType, tl, br);
        SetUV(tl, br);
    }
    if (m_enemyType == ENEMY_UNSTABLE || m_enemyType == ENEMY_GOLDEN) {
        m_countdownLabel->m_colour = {0, 0, 0, 0xff};
        m_countdownLabel->UpdateColour();
    } else if (m_enemyType == ENEMY_DEVOURER || m_enemyType == ENEMY_KAMIKAZE) {
        m_countdownLabel->m_colour = {0xff, 0xff, 0, 0xff};
        m_countdownLabel->UpdateColour();
    }
    if (m_enemyType == ENEMY_NORMAL ||
        CharacterClass::GetMortalEnemyType(CurrentClass(m_game).m_race) != m_enemyType) {
        m_mortalGlowVisible = false;
    } else {
        m_mortalGlowVisible = true;
        m_mortalGlowSpin = Rand(30.0f, 45.0f, RAND_COSMETIC);
    }
}

// Right-aligned numbers (x = 0.0752841 - width * scale, attack/armour 1 nudged left by 0.005).
void Tile::UpdateText(bool attack, bool armour, bool health) {
    char text[16];   // the original prints into a static buffer (0x00089768)
    if (attack) {
        std::sprintf(text, "%d", GetAttack());
        m_attackLabel->SetText(text, -1);
        float nudge = m_attack == 1 ? 0.005f : 0.0f;
        m_attackLabel->m_position.x = (0.07528409f - m_attackLabel->m_width * m_attackLabel->m_scale.x) - nudge;
    }
    if (armour) {
        std::sprintf(text, "%d", m_armour);
        m_armourLabel->SetText(text, -1);
        float nudge = m_armour == 1 ? 0.005f : 0.0f;
        m_armourLabel->m_position.x = (0.07528409f - m_armourLabel->m_width * m_armourLabel->m_scale.x) - nudge;
    }
    if (health) {
        std::sprintf(text, "%d", m_health);
        m_healthLabel->SetText(text, -1);
        m_healthLabel->m_position.x = 0.07528409f - m_healthLabel->m_width * m_healthLabel->m_scale.x;
    }
}

void Tile::UpdateValues() {
    UpdateText(true, true, true);
    m_renderOffsetX = (m_attack < 10 && m_armour < 10 && m_health < 200) ? -0.01f : -0.02f;
    UpdateCountdown();
}

void Tile::UpdateCountdown() {
    char text[16];   // static buffer 0x00089758 in the original
    std::sprintf(text, "%d", m_countdown);
    m_countdownLabel->SetText(text, -1);
    Label* l = m_countdownLabel;
    float x = m_renderOffsetX + (l->m_width * l->m_scale.x) * -0.5f;
    if (m_enemyType == ENEMY_DEVOURER) x = x - 0.009375f;
    l->m_position.x = x;
}

void Tile::AddHealth(int amount) {
    m_health += amount;
    UpdateText(false, false, true);
    if (m_maxHealth < m_health) m_maxHealth = m_health;
}

// Elves halve the attack of poisoned enemies (attack > 1).
int Tile::GetAttack() {
    int attack = m_attack;
    if (attack > 1 && m_poison[0] > 0 && CurrentClass(m_game).m_race == RACE_ELF) {
        attack >>= 1;
    }
    return attack;
}

void Tile::AddPoison(int amount) {
    if (m_poison[0] == 0) {
        m_poisonFadeActive = true;
        m_poisonFade = 0.0f;
        m_poisonFadeOut = false;
    }
    m_poison[0] += amount;
    m_poison[1] += amount;
    m_poison[2] += amount;
}

int Tile::GetPoison() {
    int damage = m_poison[0];
    if (damage != 0) {
        int next = m_poison[1];
        m_poison[1] = m_poison[2];
        m_poison[0] = next;
        m_poison[2] = 0;
        if (next == 0) {
            m_poisonFadeActive = false;
            m_poisonFade = 0.0f;
            m_poisonFadeOut = true;
        }
    }
    return damage;
}

// Once per turn: the special enemy's ability (countdown enemies count down first).
void Tile::ActivateAbility() {
    if (m_abilityUsed) return;
    m_abilityUsed = true;
    Game* g = m_game;
    switch (m_enemyType) {
        case ENEMY_GOLDEN:
            m_countdown -= 1;
            UpdateCountdown();
            if (m_countdown == 0) g->RunAwayEnemy(this);
            break;
        case ENEMY_HEALER: g->HealOrBless(this); break;
        case ENEMY_ACIDIC: g->MakeAcidPotion(this); break;
        case ENEMY_FLAMING: g->IgniteTiles(this); break;
        case ENEMY_FROZEN: g->FreezeTiles(this); break;
        case ENEMY_KAMIKAZE:
            m_countdown -= 1;
            UpdateCountdown();
            if (m_countdown == 0) g->ExplodeEnemy(this);
            break;
        case ENEMY_OVERSEER: g->ConvertSwordToSkull(this); break;
        case ENEMY_THIEF: g->StealGold(this); break;
        case ENEMY_CHAOTIC: g->JumpEnemy(this); break;
        case ENEMY_ASSASSIN: g->StabAttack(this); break;
        case ENEMY_DOPPELGANGER: g->MimicDefence(this); break;
        case ENEMY_ALCHEMIST: g->ConvertRandomTiles(this); break;
        case ENEMY_UNSTABLE:
            m_countdown -= 1;
            UpdateCountdown();
            if (m_countdown == 0) g->TeleportEnemy();
            break;
        case ENEMY_TRAMPLING: g->BreakRandomShieldSword(this); break;
        case ENEMY_GREASY: g->LowerSkulls(this); break;
        case ENEMY_ALPHA: g->AttractSkulls(this); break;
        case ENEMY_MAGNETIC: g->AttractShieldsSwords(this); break;
        case ENEMY_BRUTE: g->ThrowSkull(this); break;
        case ENEMY_DEVOURER:
            m_countdown -= 1;
            if (m_countdown == 0) {
                g->DestroyUpgrade(this);
                m_countdown = 5;
            }
            UpdateCountdown();
            break;
        case ENEMY_FLEEING: g->FleeUp(this); break;
        case ENEMY_LICH:
            // a dead lich (countdown 1) rises again the turn after it died
            if (m_countdown != 0) {
                if (!m_lichJustDied) {
                    m_countdown = 0;
                    GenerateStats(g->m_turn, g->m_difficulty);
                    UpdateEnemyUV();
                    UpdateValues();
                    g->Resurrect(this);
                } else {
                    m_lichJustDied = false;
                }
            }
            break;
        default: break;
    }
}

// How an attack in the chain at `it` hits this enemy: blocked, normal or uppercut (dwarves, when
// the chain comes from below and continues upwards in the same column).
int Tile::GetDamageType(std::list<Tile*>& chain, std::list<Tile*>::iterator& it) {
    if (m_type != TILE_SKULL || (m_enemyType == ENEMY_LICH && m_countdown != 0)) return DAMAGE_NONE;
    if (m_game->m_turn < 5 && CurrentClass(m_game).HaveFlaw(FLAW_CONFUSED)) return DAMAGE_BLOCKED;
    std::list<Tile*>::iterator next = it;
    ++next;
    if (m_enemyType == ENEMY_ARMOURED) {
        if (next != chain.end() && (*next)->m_row < m_row) return DAMAGE_NORMAL;
    } else {
        RaceType race = CurrentClass(m_game).m_race;
        if (race == RACE_DWARF && next != chain.end()) {
            if (m_column == (*next)->m_column && (*next)->m_row < m_row && it != chain.begin()) {
                std::list<Tile*>::iterator prev = it;
                --prev;
                if (m_column == (*prev)->m_column && m_row < (*prev)->m_row) return DAMAGE_UPPERCUT;
            }
        }
        if (m_enemyType == ENEMY_CHAOTIC && CharacterClass::GetMortalEnemyType(race) == ENEMY_CHAOTIC) {
            return DAMAGE_BLOCKED;
        }
        if (m_enemyType == ENEMY_MEAT_SHIELD || m_game->GetEnemyCount(ENEMY_MEAT_SHIELD) < 1) return DAMAGE_NORMAL;
    }
    return DAMAGE_BLOCKED;
}

// Whether this tile can extend the chain: same type, swords <-> skulls, after an arrow any skull
// or sword, coins <-> a dead lich; chain tiles always, trophies never, frozen non-skulls never.
bool Tile::CanMatchType(std::list<Tile*>& chain, bool ignoreArrows) {
    if (m_frozen && m_type != TILE_SKULL) return false;
    if (chain.empty()) return true;
    if (m_specialType == SPECIAL_TILE_CHAIN) return true;
    if (m_specialType == SPECIAL_TILE_TROPHY) return false;
    Tile* last = chain.back();
    if (!ignoreArrows && last->m_type == TILE_SWORD && last->m_arrow) return true;
    if (last->m_specialType == SPECIAL_TILE_CHAIN) return true;
    if (last->m_specialType == SPECIAL_TILE_TROPHY) return false;
    Tile* beforeLast = nullptr;
    if (chain.size() >= 2) {
        std::list<Tile*>::iterator i = chain.end();
        --i;
        --i;
        beforeLast = *i;
    }
    if (beforeLast != nullptr && beforeLast->m_type == TILE_SWORD && beforeLast->m_arrow) {
        if (IsUndeadLich(this)) return false;
        return m_type == TILE_SKULL || m_type == TILE_SWORD;
    }
    if (IsUndeadLich(this)) {
        if (last->m_type == TILE_COIN) return true;
        if (last->m_type == TILE_SKULL && last->m_enemyType == ENEMY_LICH) return last->m_countdown != 0;
        return false;
    }
    if (IsUndeadLich(last)) {
        if (m_type == TILE_COIN) return true;
        if (m_type != TILE_SKULL) return false;
        if (m_enemyType != ENEMY_LICH) return false;
        return m_countdown != 0;
    }
    if (m_type == last->m_type) return true;
    if (last->m_type == TILE_SWORD) return m_type == TILE_SKULL;
    if (last->m_type == TILE_SKULL) return m_type == TILE_SWORD;
    return false;
}

// Tiles a defeated special enemy turns into: 15..25 of them, coins and skulls (Healer: also
// potions, Armoured: also shields, Golden: all coins, Thief: more coins).
void Tile::GetRewards(std::list<TileType>& rewards) {
    int n = RandI(15, 25, RAND_TILE);
    auto add = [&rewards](int count, TileType type) {
        for (int i = 0; i < count; ++i) rewards.push_back(type);
    };
    switch (m_enemyType) {
        case ENEMY_GOLDEN:
            add(n, TILE_COIN);
            break;
        case ENEMY_HEALER:
        case ENEMY_ARMOURED: {
            int first = RandI(n / 4, n / 3, RAND_TILE);
            int rest = n - first;
            int coins = RandI(rest * 3 / 4, rest / 4, RAND_TILE);
            add(first, m_enemyType == ENEMY_HEALER ? TILE_POTION : TILE_SHIELD);
            add(coins, TILE_COIN);
            add(rest - coins, TILE_SKULL);
            break;
        }
        case ENEMY_THIEF: {
            int coins = RandI(n * 3 / 4, n / 2, RAND_TILE);
            add(coins, TILE_COIN);
            add(n - coins, TILE_SKULL);
            break;
        }
        default: {
            int coins = RandI(n * 3 / 4, n / 3, RAND_TILE);
            add(coins, TILE_COIN);
            add(n - coins, TILE_SKULL);
            break;
        }
    }
}

void Tile::MakeMagicSword() {
    SetColour(GLcolour4{0x96, 0x96, 0xff, 0xff});
    m_magicSword = true;
}

void Tile::MakeArrow() {
    SetUV(GLfloat2{0.5517578125f, 0.4482421875f}, GLfloat2{0.603515625f, 0.5f});
    m_arrow = true;
}

void Tile::MakeManaPotion() {
    SetCellUV(*this, m_variant, m_type, 0.5009765625f, true);
    m_manaPotion = true;
}

void Tile::MakeAcidPotion() {
    SetCellUV(*this, m_variant, m_type, 0.552734375f, true);
    m_acidPotion = true;
}

void Tile::MakeExplosivePotion() {
    SetCellUV(*this, m_variant, m_type, 0.6044921875f, true);
    m_explosivePotion = true;
}

void Tile::MakeBroken() {
    if (static_cast<unsigned>(m_type - TILE_SHIELD) < 2) {   // shields and swords
        m_broken = true;
        if (m_type == TILE_SWORD) {
            SetUV(GLfloat2{0.4140625f, 0.759765625f}, GLfloat2{0.4658203125f, 0.8115234375f});
        } else {
            SetUV(GLfloat2{0.4140625f, 0.7080078125f}, GLfloat2{0.4658203125f, 0.759765625f});
        }
    }
}

void Tile::SetFlameVisibility(bool visible, int damage, bool fireBreath) {
    if (!visible) {
        m_overlayVisible = false;
        m_burning = false;
        m_burnIsFireBreath = fireBreath;
        return;
    }
    m_burnDamage = m_burning ? m_burnDamage + damage : damage;
    m_burnIsFireBreath = fireBreath;
    m_overlayVisible = true;
    m_frozen = false;
    m_burning = true;
    m_overlay.SetUV(GLfloat2{0.4150390625f, 0.55859375f}, GLfloat2{0.4501953125f, 0.603515625f});
    m_overlay.SetSize(GLfloat2{0.1125f, 0.14375f});
    m_overlay.SetAlpha(200);
}

void Tile::SetFreezeVisibility(bool visible) {
    m_overlayVisible = visible;
    m_frozen = visible;
    if (visible) {
        m_burning = false;
        m_overlay.SetUV(GLfloat2{0.3212890625f, 0.3896484375f}, GLfloat2{0.3701171875f, 0.435546875f});
        m_overlay.SetSize(GLfloat2{0.15625f, 0.146875f});
        m_overlay.SetAlpha(0xff);
    }
}

void Tile::Dim() {
    SetAlpha(0x96);
    m_overlay.SetAlpha(0x96);
    m_mortalGlow.SetAlpha(0x96);
    if (m_type == TILE_SKULL) {
        for (Label* l : {m_attackLabel, m_armourLabel, m_healthLabel, m_countdownLabel}) {
            l->m_colour.a = 0x96;
            l->UpdateColour();
        }
    }
}

void Tile::Undim() {
    SetAlpha(0xff);
    m_mortalGlow.SetAlpha(0xff);
    m_overlay.SetAlpha(m_burning ? 200 : 0xff);
    if (m_type == TILE_SKULL) {
        for (Label* l : {m_attackLabel, m_armourLabel, m_healthLabel, m_countdownLabel}) {
            l->m_colour.a = 0xff;
            l->UpdateColour();
        }
    }
}

bool Tile::IsBeingSelected(const GLfloat2& point) {
    float dx = m_position.x - point.x;
    float dy = m_position.y - point.y;
    return std::sqrt(dy * dy + dx * dx) < m_baseSize.y * 0.8f;
}

void Tile::Process(float dt) {
    float speed = m_fallSpeed + dt * m_fallAcceleration;
    float y = m_position.y + dt * speed;
    m_fallSpeed = speed;
    m_position.y = y;
    if (m_targetPosition.y <= y) {
        m_position.y = m_targetPosition.y;
        m_falling = false;
        m_fallSpeed = 1.0f;
    }
}

// Spins the mortal-enemy glow; fades the poison tint (white <-> (169, 76, 202)) in or out.
void Tile::ProcessEffects(float dt) {
    if (m_selected != m_popSelected) {   // port: the selection pop (cosmetic)
        m_popSelected = m_selected;
        m_popTime = m_selected && m_game != nullptr && m_game->m_touchIsMouse ? kPopDuration : 0.0f;
    } else if (m_popTime > 0.0f) {
        m_popTime = m_popTime > dt ? m_popTime - dt : 0.0f;
    }
    if (m_mortalGlowVisible) {
        m_mortalGlow.m_rotation = m_mortalGlow.m_rotation + dt * m_mortalGlowSpin;
    }
    if (!m_poisonFadeActive && !m_poisonFadeOut) return;
    bool out = m_poisonFadeOut;
    float fade = m_poisonFade + dt * 3.0f;
    m_poisonFade = fade;
    if (1.0f <= fade) {
        m_poisonFade = 1.0f;
        fade = 1.0f;
    }
    float r = out ? Interpolate(255.0f, 169.0f, fade) : Interpolate(169.0f, 255.0f, fade);
    float g = m_poisonFadeOut ? Interpolate(255.0f, 76.0f, m_poisonFade) : Interpolate(76.0f, 255.0f, m_poisonFade);
    float b = m_poisonFadeOut ? Interpolate(255.0f, 202.0f, m_poisonFade) : Interpolate(202.0f, 255.0f, m_poisonFade);
    GLcolour4 c;
    c.a = m_vertices[0].colour.a;
    c.r = static_cast<uint8_t>(FloatToIntArm(r));
    c.g = static_cast<uint8_t>(FloatToIntArm(g));
    c.b = static_cast<uint8_t>(FloatToIntArm(b));
    SetColour(c);
    if (1.0f <= m_poisonFade) {
        m_poisonFadeActive = false;
        m_poisonFadeOut = false;
        UpdateText(true, false, false);
    }
}

// Skulls are shifted by m_renderOffsetX (not a dead lich) and drawn over their mortal glow.
void Tile::Render() {
    sdlPushMatrix();   // port: the idle shake's offset (0 otherwise)
    sdlTranslatef(m_idleOffset.x, m_idleOffset.y, 0.0f);
    if (m_type != TILE_SKULL) {
        RenderPopped();
        sdlPopMatrix();
        return;
    }
    sdlPushMatrix();
    if (!IsUndeadLich(this)) sdlTranslatef(m_renderOffsetX, 0.0f, 0.0f);
    if (m_mortalGlowVisible) {
        m_mortalGlow.m_position = m_position;
        m_mortalGlow.Render();
    }
    RenderPopped();
    sdlPopMatrix();
    sdlPopMatrix();
}

// Without a pop just Quad::Render. Popping: drawn up to 25% larger, easing back to its size, then
// again additively with a fading alpha (the flash); the scale and alphas are restored. The idle
// wobble's angle is added to the rotation while drawing.
void Tile::RenderPopped() {
    const float rotation = m_rotation;
    m_rotation = rotation + m_idleRotation;
    RenderPoppedQuad();
    m_rotation = rotation;
}

void Tile::RenderPoppedQuad() {
    if (m_popTime <= 0.0f) {
        Quad::Render();
        return;
    }
    float left = m_popTime / kPopDuration;   // 1 -> 0
    const GLfloat2 scale = m_scale;
    float f = 1.0f + 0.25f * left * left;
    m_scale = GLfloat2{scale.x * f, scale.y * f};
    Quad::Render();
    uint8_t alphas[4];
    for (int i = 0; i < 4; ++i) {
        alphas[i] = m_vertices[i].colour.a;
        m_vertices[i].colour.a = static_cast<uint8_t>(static_cast<float>(alphas[i]) * 0.6f * left);
    }
    sdlBlendFunc(GL_SRC_ALPHA, GL_ONE);
    Quad::Render();
    sdlBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (int i = 0; i < 4; ++i) m_vertices[i].colour.a = alphas[i];
    m_scale = scale;
}

void Tile::RenderText() {
    if (m_type == TILE_SKULL && (m_enemyType != ENEMY_LICH || m_countdown == 0)) {
        sdlPushMatrix();
        sdlTranslatef(m_position.x + m_idleOffset.x, m_position.y + m_idleOffset.y, 0.0f);   // port: + the idle shake
        m_attackLabel->Render();
        m_armourLabel->Render();
        m_healthLabel->Render();
        if (m_enemyType != ENEMY_LICH && m_countdown > 0) m_countdownLabel->Render();
        sdlPopMatrix();
    }
}

void Tile::RenderOverlay() {
    if (m_overlayVisible) {
        m_overlay.m_position = GLfloat2{m_position.x + m_idleOffset.x, m_position.y + m_idleOffset.y};   // port: + the idle shake
        m_overlay.Render();
    }
}

void Tile::RenderCross() {
    if (m_crossVisible) {
        m_cross.m_position = GLfloat2{m_position.x + m_idleOffset.x, m_position.y + m_idleOffset.y};   // port: + the idle shake
        m_cross.Render();
    }
}

namespace {
GLfloat2 LabelAnchor(const Tile* t, const Label* l) {
    GLfloat2 p = t->m_position;
    p.x = p.x + (l->m_position.x + (l->m_width * l->m_scale.x) * 0.5f);
    p.y = p.y + (l->m_position.y - 0.015f);
    return p;
}
}  // namespace

GLfloat2 Tile::GetDefenceLabelPosition() { return LabelAnchor(this, m_armourLabel); }
GLfloat2 Tile::GetHealthLabelPosition() { return LabelAnchor(this, m_healthLabel); }
GLfloat2 Tile::GetAttackLabelPosition() { return LabelAnchor(this, m_attackLabel); }

GLfloat2 Tile::GetEyePosition(int right) {
    const GLfloat2* eyes = right == 0 ? g_leftEyePositions : g_rightEyePositions;
    return {eyes[m_variant].x + m_position.x, eyes[m_variant].y + m_position.y};
}

// Bytes: the variant (potion: 1 explosive, 2 mana, 3 acid; sword: 1 magic, 2 broken, 3 arrow;
// shield: 1 broken; special: its type; skulls/coins: none), then 1 frozen / 2 burning / 0 (+ burn
// damage); skulls: attack, armour, health, max health (ints), enemy type, countdown, promoted by
// Big Game, then the poison bytes up to and including the first 0.
void Tile::Serialise(GameState* state) {
    switch (m_type) {
        case TILE_POTION:
            state->WriteChar(m_explosivePotion ? 1 : m_manaPotion ? 2 : m_acidPotion ? 3 : 0);
            break;
        case TILE_SWORD:
            state->WriteChar(m_magicSword ? 1 : m_broken ? 2 : m_arrow ? 3 : 0);
            break;
        case TILE_SHIELD:
            state->WriteChar(m_broken ? 1 : 0);
            break;
        case TILE_SPECIAL:
            state->WriteChar(static_cast<uint8_t>(m_specialType));
            break;
        default:
            break;
    }
    state->WriteChar(m_frozen ? 1 : m_burning ? 2 : 0);
    if (m_burning) state->WriteChar(static_cast<uint8_t>(m_burnDamage));
    if (m_type == TILE_SKULL) {
        state->WriteInteger(m_attack);
        state->WriteInteger(m_armour);
        state->WriteInteger(m_health);
        state->WriteInteger(m_maxHealth);
        state->WriteChar(static_cast<uint8_t>(m_enemyType));
        state->WriteChar(static_cast<uint8_t>(m_countdown));
        state->WriteChar(m_promotedByBigGame ? 1 : 0);
        for (int i = 0; i < 3; ++i) {
            state->WriteChar(static_cast<uint8_t>(m_poison[i]));
            if (m_poison[i] == 0) return;
        }
    }
}

// Poison values after the first 0 keep their old values (as in the original).
void Tile::Deserialise1(GameState* state) {
    if (m_type != TILE_SKULL) return;
    m_attack = state->ReadChar();
    m_armour = state->ReadChar();
    m_health = state->ReadChar();
    UpdateValues();
    m_enemyType = ENEMY_NORMAL;
    for (int i = 0; i < 3; ++i) {
        m_poison[i] = state->ReadChar();
        if (m_poison[i] == 0) break;
    }
    if (m_poison[0] != 0) {
        m_poisonFadeActive = true;
        m_poisonFade = 1.0f;
    }
}

// The type must be set before. Shields store their broken flag only from version 1 on.
void Tile::Deserialise2(GameState* state, int version) {
    if (m_type == TILE_POTION) {
        int v = state->ReadChar();
        if (v == 1) MakeExplosivePotion();
        else if (v == 2) MakeManaPotion();
        else if (v == 3) MakeAcidPotion();
    } else if (m_type == TILE_SWORD) {
        int v = state->ReadChar();
        if (v == 1) MakeMagicSword();
        else if (v == 2) MakeBroken();
        else if (v == 3) MakeArrow();
    } else if (version >= 1 && m_type == TILE_SHIELD) {
        if (state->ReadChar() == 1) MakeBroken();
    } else if (m_type == TILE_SPECIAL) {
        SetSpecialType(static_cast<SpecialTileType>(state->ReadChar()));
    }
    int overlay = state->ReadChar();
    if (overlay == 1) SetFreezeVisibility(true);
    else if (overlay == 2) SetFlameVisibility(true, 0, false);
    if (m_burning) m_burnDamage = state->ReadChar();
    if (m_type == TILE_SKULL) {
        m_attack = state->ReadInteger();
        m_armour = state->ReadInteger();
        m_health = state->ReadInteger();
        m_maxHealth = state->ReadInteger();
        UpdateValues();
        m_enemyType = static_cast<EnemyType>(state->ReadChar());
        m_countdown = state->ReadChar();
        m_promotedByBigGame = state->ReadChar() == 1;
        UpdateCountdown();
        UpdateEnemyUV();
        for (int i = 0; i < 3; ++i) {
            m_poison[i] = state->ReadChar();
            if (m_poison[i] == 0) break;
        }
        if (m_poison[0] != 0) {
            m_poisonFadeActive = true;
            m_poisonFade = 1.0f;
        }
    }
}

// Special enemies: 8 per row from row 0.65625 (index = type - 1).
void Tile::GetEnemyUV(EnemyType type, GLfloat2& topLeft, GLfloat2& bottomRight) {
    float u;
    if (type == ENEMY_NORMAL) {
        u = 0.0f;
        topLeft = {0.0f, kCell};
    } else {
        int index = type - ENEMY_BOSS;
        u = static_cast<float>(index % 8) * kCell;
        topLeft.x = u;
        topLeft.y = static_cast<float>(index / 8) * kCell + 0.65625f;
    }
    bottomRight.x = u + kCell;
    bottomRight.y = topLeft.y + kCell;
}

const char* Tile::GetEnemyName(EnemyType type) {
    return g_enemyNames[type];
}

const char* Tile::GetEnemyDescription(EnemyType type) {
    return g_enemyText[type];
}

void Tile::GetMortalGlowUV(GLfloat2& topLeft, GLfloat2& bottomRight) {
    topLeft = {0.0009765625f, 0.904296875f};
    bottomRight = {0.052734375f, 0.9609375f};
}

// A random variant of the type's cell (for icons).
void Tile::GetTileUV(int type, GLfloat2& topLeft, GLfloat2& bottomRight) {
    int variant = RandI(0, 7, RAND_COSMETIC);
    topLeft.y = static_cast<float>(type) * kCell;
    float u = static_cast<float>(variant) * kCell;
    topLeft.x = u;
    bottomRight.x = u + kCell;
    bottomRight.y = topLeft.y + kCell;
}
