// Game: the ported methods (slice 1: board geometry and queries, stats, items, skills, resources,
// achievements and sounds, new game, save states). See game.h.
#include "game.h"

#include "item.h"
#include "item_upgrade.h"
#include "rand.h"
#include "skill.h"
#include "util.h"


namespace {
CharacterClass& Current(Game* g) { return g->m_classes[g->m_currentClass]; }
}  // namespace

// ================================================================ construction

float Game::s_viewWidth = 0.0f;
float Game::s_viewHeight = 0.0f;

// The members construct themselves; then the view size (static), the clock (InitialiseTimeSettings
// is empty), the renderer, the item slot list. The original ends with SetSoundState(true).
Game::Game(float viewWidth, float viewHeight)
    : m_interface(this), m_fpsLabel(&m_fonts[0]), m_introScreen(this), m_mainMenu(this), m_gameOverScreen(this), m_scoreScreen(this), m_helpMenu(this), m_nameScreen(this), m_unlockMenu(this), m_settingsScreen(this), m_achievementsScreen(this), m_infoPanel(this), m_toast(this) {
    s_viewWidth = viewWidth;
    s_viewHeight = viewHeight;
    m_lastTime = static_cast<float>(GetTime());
    SetupRenderer();
    for (Item*& slot : m_items) m_itemSlots.push_back(&slot);
    SetSoundState(true);
}

Game::~Game() {
    for (Item* item : m_items) delete item;
    for (auto& row : m_board) {
        for (Tile& tile : row) tile.DeleteLabels();   // (Tile's own destructor leaves them)
    }
    for (Skill* skill : m_skills) delete skill;
}

void Game::Start(bool soundEnabled) {
    SetRandSeed(GetUnixTime(), RAND_TYPE_COUNT);
    FlushRandSeeds();
    SetSoundState(soundEnabled);
    m_nameScreen.Initialise();
    m_scoreScreen.Initialise();
    m_tileSize = 0.165625f;
    m_halfTileSize = 0.0828125f;
    CalculateTilePositions();
    const float px = 1.0f / 1024.0f;
    m_simpleLabelB904.SetColour(10);
    m_simpleLabelB904.m_lifetime = 0.2f;
    m_simpleLabelC388.SetColour(0x11);
    m_simpleLabelC388.m_lifetime = 0.2f;
    m_simpleLabel14344.SetColour(0x12);
    m_simpleLabel14344.m_lifetime = 0.2f;
    m_simpleLabel146b8.SetColour(0x13);
    m_simpleLabel146b8.m_lifetime = 0.2f;
    m_attackBanner.SetSize(GLfloat2{0.778125f, 0.075f});
    m_attackBanner.m_position = GLfloat2{0.5f, 0.53f};
    m_attackBanner.SetColour(GLcolour4{0xff, 0, 0, 0xff});
    m_attackBanner.SetUV(GLfloat2{197 * px, 287 * px}, GLfloat2{446 * px, 311 * px});
    m_counterBanner.SetSize(GLfloat2{0.575f, 0.075f});
    m_counterBanner.m_position = GLfloat2{0.5f, 0.53f};
    m_counterBanner.SetColour(GLcolour4{0xff, 0xff, 0, 0xff});
    m_counterBanner.SetUV(GLfloat2{269 * px, 312 * px}, GLfloat2{453 * px, 336 * px});
    m_counterBanner.SetAlpha(0);
    m_incomingLabelY = 0.6675f;
    m_incomingDamageLabel.SetColour(0xb);
    m_incomingDamageLabel.m_lifetime = 0.2f;
    m_incomingDamageLabel.Initialise(2, GLfloat2{0.5f, m_incomingLabelY}, 0, 1.0f);
    m_incomingPoisonLabel.SetColour(0xf);
    m_incomingPoisonLabel.m_lifetime = 0.2f;
    m_incomingPoisonLabel.Initialise(7, GLfloat2{0.5f, m_incomingLabelY + 0.05f}, 0, 1.0f);
    m_attackShade.SetSize(GLfloat2{2.0f, 2.0f});
    m_attackShade.m_position = GLfloat2{0.5f, 0.5f};
    m_attackShade.SetColour(GLcolour4{0, 0, 0, 0x78});
    m_shieldShatterLeft.SetSize(GLfloat2{0.034375f, 0.0625f});
    m_shieldShatterLeft.SetUV(GLfloat2{0.4765625f, 0.4501953125f}, GLfloat2{0.4873046875f, 0.4697265625f});
    m_shieldShatterRight.SetSize(GLfloat2{0.034375f, 0.0625f});
    m_shieldShatterRight.SetUV(GLfloat2{0.48828125f, 0.4501953125f}, GLfloat2{0.4990234375f, 0.4697265625f});
    for (Skill*& skill : m_skills) skill = new Skill(this);
    for (int c = 0; c < CLASS_COUNT; ++c) {
        m_classes[c].Initialise(static_cast<ClassType>(c), this);
    }
    if (m_classState.GetStateSize() > 0) DeserialiseClasses();
    if (m_classes[m_currentClass].m_level == 0) m_currentClass = 0;
    m_nameScreen.SelectClass(m_classes[m_currentClass]);
    m_currentClassChanged = true;
}

// ================================================================ board

// Grid positions: x = 0.003125 + (column + 0.5) * size, y = 0.246875 + (row + 0.5) * size + the
// board offset (challenges are shifted down by 0.015625). Also sizes, places and labels the tiles.
void Game::CalculateTilePositions() {
    m_boardOffsetY = m_challenge ? 0.015625f : 0.0f;
    float half = m_tileSize * 0.5f;
    for (int row = 0; row < 6; ++row) {
        float fy = static_cast<float>(row) + 0.5f;
        for (int col = 0; col < 6; ++col) {
            float size = m_tileSize;
            GLfloat2& p = m_gridPositions[row][col];
            p.x = 0.003125f + (static_cast<float>(col) + 0.5f) * size;
            p.y = (0.246875f + fy * m_tileSize) + m_boardOffsetY;
            Tile& t = m_board[row][col];
            t.m_baseSize = {size, half};
            t.SetSize(GLfloat2{size, size});
            t.m_column = col;
            t.m_row = row;
            t.m_position = p;
            t.InitialiseLabels(this);
        }
    }
}

// Without animation the tile is put at its grid position (rows < 0: above the board); with it, the
// grid position becomes its fall target.
void Game::PositionTile(Tile* tile, int column, int row, bool animate) {
    if (animate) {
        tile->m_targetPosition = m_gridPositions[row][column];
        tile->m_falling = true;
    } else if (row < 0) {
        float size = m_tileSize;
        tile->m_position.y = (m_boardOffsetY + 0.246875f) + size * (static_cast<float>(row) + 0.5f);
        tile->m_position.x = 0.003125f + size * (static_cast<float>(column) + 0.5f);
    } else {
        tile->m_position = m_gridPositions[row][column];
    }
}

void Game::SwapTiles(int column1, int row1, int column2, int row2) {
    Tile* second = &m_board[row2][column2];
    PositionTile(second, column1, row1, false);
    Tile* first = &m_board[row1][column1];
    PositionTile(first, column2, row2, false);
    second->m_falling = true;
    second->m_targetPosition = m_gridPositions[row1][column1];
    first->m_falling = true;
    first->m_targetPosition = m_gridPositions[row2][column2];
    second->m_column = column1;
    second->m_row = row1;
    first->m_column = column2;
    first->m_row = row2;
    Tile copy(*second);
    *second = *first;
    *first = copy;
}

Tile* Game::GetTileAtPosition(const GLfloat2& position) {
    float top = m_boardOffsetY + 0.246875f;
    float y = position.y;
    if (!(y < top) && !(y >= top + 1.0f)) {   // (as the original's branches: NaN passes both tests)
        unsigned col = static_cast<unsigned>(FloatToIntArm(position.x / m_tileSize));
        int row = FloatToIntArm((y - top) / m_tileSize);
        if (col < 6 && row >= 0 && row < 6) return &m_board[row][col];
    }
    return nullptr;
}

// The nearest row above `row` whose tile in `column` is not matched, -1 if none.
int Game::GetNearestUnmatchedTile(int column, int row) {
    for (int r = row - 1; r >= 0; --r) {
        if (!m_board[r][column].m_matched) return r;
    }
    return -1;
}

// A random tile type (all coins in a treasure chamber, skulls when a sprint promotion is due, no
// skulls (or no swords) while a new board is filled), with the class's per-type extras.
void Game::RandomiseTile(Tile* tile) {
    int type;
    if (m_treasureChamberActive && !m_fillingNewBoard && !m_collectingBoard) {
        type = TILE_COIN;
    } else {
        type = RandI(0, 4, RAND_TILE);
    }
    if (m_challenge && m_mode == MODE_SPRINT && IsTimeToPromote()) type = TILE_SKULL;
    if (m_fillingNewBoard) {
        if (!m_newBoardAvoidsSwords) {
            while (type == TILE_SKULL) type = RandI(0, 4, RAND_TILE);
        } else {
            while (type == TILE_SWORD) type = RandI(0, 4, RAND_TILE);
        }
    }
    tile->SetType(static_cast<TileType>(type), m_turn, m_difficulty);
    m_tilesSpawned += 1;
    if (tile->m_type == TILE_SKULL) m_skullsSpawned += 1;
    switch (type) {
        case TILE_POTION:
            if (Current(this).HavePerk(PERK_GIFTED) && RandI(1, 100, RAND_TILE) < 6) tile->MakeManaPotion();
            break;
        case TILE_SKULL:
            if (m_trapActive) {
                tile->m_health = 1;
                tile->UpdateText(false, false, true);
                if (tile->m_enemyType != ENEMY_NORMAL) QueueAchievement(ACH_PREMONITION, 100.0f);
            }
            if (CharacterClass::GetMortalEnemyType(Current(this).m_race) == ENEMY_CORROSIVE &&
                tile->m_enemyType == ENEMY_CORROSIVE) {
                m_armour = 0;
                if (m_interface.m_armour != 0) {
                    m_interface.m_armour = 0;
                    m_interface.UpdateShieldValue();
                }
                InialiseShieldShatter();
                QueueSound(SOUND_ARMOUR_BREAK);
            }
            if (tile->m_enemyType == ENEMY_FLEEING) JumpDown(tile);
            break;
        case TILE_SHIELD:
            if (Current(this).HaveFlaw(FLAW_CLUMSY) && RandI(1, 100, RAND_TILE) < 4) tile->MakeBroken();
            break;
        case TILE_SWORD:
            if (Current(this).HavePerk(PERK_AGILE) && RandI(1, 100, RAND_TILE) < 6) tile->MakeArrow();
            break;
        default:
            break;
    }
}

int Game::GetTileCount(TileType type) {
    int n = 0;
    for (auto& row : m_board) {
        for (Tile& t : row) {
            if (t.m_type == type) ++n;
        }
    }
    return n;
}

int Game::GetSpecialEnemyCount() {
    int n = 0;
    for (auto& row : m_board) {
        for (Tile& t : row) {
            if (t.m_type == TILE_SKULL && t.m_enemyType != ENEMY_NORMAL) ++n;
        }
    }
    return n;
}

int Game::GetEnemyCount(EnemyType type) {
    int n = 0;
    for (auto& row : m_board) {
        for (Tile& t : row) {
            if (t.m_type == TILE_SKULL && t.m_enemyType == type) ++n;
        }
    }
    return n;
}

int Game::GetNormalPotionCount() {
    int n = 0;
    for (auto& row : m_board) {
        for (Tile& t : row) {
            if (t.m_type == TILE_POTION && !t.m_explosivePotion && !t.m_manaPotion && !t.m_acidPotion) ++n;
        }
    }
    return n;
}

int Game::GetNormalSwordCount() {
    int n = 0;
    for (auto& row : m_board) {
        for (Tile& t : row) {
            if (t.m_type == TILE_SWORD && !t.m_magicSword && !t.m_arrow) ++n;
        }
    }
    return n;
}

// Vengeful enemies gain the chain's kill count as attack. (The original walks the board column
// by column.)
void Game::AddRevengeHealth() {
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            Tile& t = m_board[row][col];
            if (!t.m_matched && t.m_type == TILE_SKULL && t.m_enemyType == ENEMY_VENGEFUL) {
                t.m_attack += m_chainKills;
                t.UpdateText(true, false, false);
                AddSimpleLabel(0x1b, &t.m_position, m_chainKills, 0x23, 1.0f);
                QueueSound(SOUND_SKULL);
            }
        }
    }
}

// Sprint challenge: due when the timer ran out, or (timer 200 = waiting) every 10th turn from turn 4
// (every 9th from turn 3 with flaw Zealous), which also resets the timer. Otherwise: timer <= 1.
bool Game::IsTimeToPromote() {
    if (!m_challenge || m_mode != MODE_SPRINT) return m_timeToPromotion == 0;
    if (m_timeToPromotion == 0) return true;
    if (m_timeToPromotion == 200) return false;
    bool due;
    if (!Current(this).HaveFlaw(FLAW_ZEALOUS)) {
        due = (m_turn - 4) % 10 == 0;
    } else {
        due = (m_turn - 3) % 9 == 0;
    }
    if (due) m_timeToPromotion = 0;
    return due;
}

void Game::SetTimeToPromotion(int turns) {
    if (!m_promotionTimerEnabled) return;
    m_timeToPromotion = (m_challenge && m_mode == MODE_SPRINT) ? 200 : turns;
    QueueSound(SOUND_SKULL);
}

void Game::SetTimeToTrophy(int turns) {
    m_timeToTrophy = turns > 29 ? 30 : turns;
}

// Two shield halves that fly apart (Corrosive enemies, ShatterDefence).
void Game::InialiseShieldShatter() {
    m_shieldShatterLeft.m_position = {0.4859375f, 1.29375f};
    m_shieldShatterLeft.SetAlpha(0xff);
    m_shieldShatterLeft.m_rotation = 0.0f;
    m_shieldShatterRight.m_position.y = 1.29375f;
    m_shieldShatterRight.m_position.x = 0.5078125f;
    m_shieldShatterRight.SetAlpha(0xff);
    m_shieldShatterRight.m_rotation = 0.0f;
    m_shieldShatterActive = true;
    m_shieldShatterTime = 0.0f;
}

// ================================================================ player

// Recomputes everything the items and attributes give. Armour keeps its distance to the maximum;
// a higher max health raises health by the difference; Quicken changes every cooldown by one.
void Game::UpdateStats() {
    int oldMaxArmour = m_maxArmour;
    int oldArmour = m_armour;
    m_weaponDamage = 0;
    m_maxArmour = 0;
    bool cautious = Current(this).HaveFlaw(FLAW_CAUTIOUS);
    m_armourDamageChance = 50;
    m_armourPiercingChance = cautious ? 30 : 50;
    bool hadQuicken = m_quicken;
    m_poisonAttack = 0;
    m_lifeLeech = 0;
    m_counterDamage = 0;
    m_attackReduction = 0;
    m_regeneration = 0;
    m_quicken = false;
    int baseMaxHealth = m_baseMaxHealth;
    m_itemStrength = 0;
    m_itemDexterity = 0;
    m_itemVitality = 0;
    m_itemLuck = 0;
    int oldBonusMaxHealth = m_bonusMaxHealth;
    int health = m_health;
    m_xpBonusRate = 0.0f;
    m_bonusMaxHealth = 0;
    m_goldBonusRate = 0.0f;
    m_bonusDamage = 0;
    m_upgradeBonusRate = 0.0f;
    m_bonusArmourAmount = 0;
    bool pretzel = m_challenge && m_mode == MODE_PRETZEL;
    if (!m_itemSlots.empty()) {
        for (Item** slot : m_itemSlots) {
            const int* s = (*slot)->m_stats;
            m_weaponDamage += s[UPGRADE_WEAPON_DAMAGE];
            m_maxArmour += s[UPGRADE_ARMOUR];
            m_armourPiercingChance += s[UPGRADE_ARMOUR_PIERCING];
            m_armourDamageChance -= s[UPGRADE_DURABILITY];
            m_poisonAttack += s[UPGRADE_POISON];
            m_lifeLeech += s[UPGRADE_LIFE_LEECH];
            m_counterDamage += s[UPGRADE_SPIKES];
            m_attackReduction += s[UPGRADE_BLUNTING];
            m_regeneration += s[UPGRADE_REGENERATE];
            if (s[UPGRADE_QUICKEN] > 0) m_quicken = true;
            m_itemStrength += s[UPGRADE_BOOST_STRENGTH];
            m_itemDexterity += s[UPGRADE_BOOST_DEXTERITY];
            m_itemVitality += s[UPGRADE_BOOST_VITALITY];
            m_itemLuck += s[UPGRADE_BOOST_LUCK];
            if (!pretzel) m_bonusMaxHealth += s[UPGRADE_HEALTH];
            m_bonusDamage += s[UPGRADE_BASE_DAMAGE];
            m_bonusArmourAmount += s[UPGRADE_REPAIR];
            m_xpBonusRate = m_xpBonusRate + ItemUpgrade::GetFloatValue(s[UPGRADE_BOOST_XP]);
            m_goldBonusRate = m_goldBonusRate + ItemUpgrade::GetFloatValue(s[UPGRADE_BOOST_GOLD]);
            m_upgradeBonusRate = m_upgradeBonusRate + ItemUpgrade::GetFloatValue(s[UPGRADE_BOOST_UP]);
        }
        if (2.0f < m_xpBonusRate) m_xpBonusRate = 2.0f;
    }
    if (2.0f < m_goldBonusRate) m_goldBonusRate = 2.0f;
    if (2.0f < m_upgradeBonusRate) m_upgradeBonusRate = 2.0f;
    if (m_armourPiercingChance > 95) m_armourPiercingChance = 95;
    if (m_armourDamageChance < 5) m_armourDamageChance = 5;
    if (!pretzel) m_bonusMaxHealth = m_itemVitality * Skill::GetStat(STAT_VIT) + m_bonusMaxHealth;
    m_bonusDamage = m_itemStrength * Skill::GetStat(STAT_STR) + m_bonusDamage;
    m_bonusHealAmount = m_itemLuck * Skill::GetStat(STAT_LUK);
    m_bonusArmourAmount = m_itemDexterity * Skill::GetStat(STAT_DEX) + m_bonusArmourAmount;
    m_bonusCoinTokenChance = m_itemLuck * Skill::GetStatChance();
    m_bonusPotionTokenChance = m_itemVitality * Skill::GetStatChance();
    m_bonusSkullTokenChance = m_itemStrength * Skill::GetStatChance();
    m_bonusShieldTokenChance = m_itemDexterity * Skill::GetStatChance();
    if (oldBonusMaxHealth != m_bonusMaxHealth) {
        int maxHealth = m_bonusMaxHealth + m_baseMaxHealth;
        if (oldBonusMaxHealth < m_bonusMaxHealth) {
            m_health = ((maxHealth - oldBonusMaxHealth) - baseMaxHealth) + health;
        } else if (maxHealth < m_health) {
            m_health = maxHealth;
        }
        if (m_interface.m_maxHealth != maxHealth) {
            m_interface.m_maxHealth = maxHealth;
            m_interface.UpdateHealthBar(Interface::BAR_HEALTH, m_interface.m_health);
        }
        if (m_health != m_interface.m_health) {
            m_interface.m_health = m_health;
            m_interface.UpdateHealthBar(Interface::BAR_HEALTH, m_health);
        }
    }
    int armour = m_maxArmour - (oldMaxArmour - oldArmour);
    m_armour = armour < 0 ? 0 : armour;
    if (m_weaponDamage != m_interface.m_swordValue) {
        m_interface.m_swordValue = m_weaponDamage;
        m_interface.UpdateSwordValue();
    }
    if (m_bonusDamage + m_baseDamage != m_interface.m_skullValue) {
        m_interface.m_skullValue = m_bonusDamage + m_baseDamage;
        m_interface.UpdateSkullValue();
    }
    if (m_armour != m_interface.m_armour) {
        m_interface.m_armour = m_armour;
        m_interface.UpdateShieldValue();
    }
    if (m_maxArmour != m_interface.m_maxArmour) {
        m_interface.m_maxArmour = m_maxArmour;
        m_interface.UpdateShieldValue();
    }
    int change;
    if (!m_quicken) {
        if (!hadQuicken) return;
        change = 1;
    } else {
        if (hadQuicken) return;
        change = -1;
    }
    for (int i = 0; i < m_skillCount; ++i) {
        Skill* s = m_skills[i];
        int cooldown = change + s->m_cooldown;
        if (cooldown <= s->m_charge) s->m_charge = cooldown;
        s->m_cooldown = cooldown;
    }
    m_interface.UpdateSkills();
}

Item* Game::GetItem(ItemType type) {
    switch (type) {
        case WEAPON: return m_items[0];
        case HELMET: return m_items[1];
        case ARMOUR: return m_items[2];
        case SHIELD: return m_items[3];
        default: return m_items[4];
    }
}

Item* Game::SetItem(Item* item) {
    int slot;
    switch (item->m_type) {
        case WEAPON: slot = 0; break;
        case HELMET: slot = 1; break;
        case ARMOUR: slot = 2; break;
        case SHIELD: slot = 3; break;
        default: slot = 4; break;
    }
    Item* old = m_items[slot];
    m_items[slot] = item;
    return old;
}

// Real skills: replaces the skill of the same type, else takes the next of the 4 slots (Quicken and
// Cunning on the 4th lower the new skill's cooldown); returns the object now unused. Stat choices
// add their bonuses and return themselves.
Skill* Game::SetSkill(Skill* skill) {
    if (static_cast<int>(skill->m_type) < 0x2a) {
        Skill* freed;
        int index = -1;
        for (int i = 0; i < m_skillCount; ++i) {
            if (m_skills[i]->m_type == skill->m_type) {
                index = i;
                break;
            }
        }
        if (index >= 0) {
            freed = m_skills[index];
            m_skills[index] = skill;
            m_interface.PositionSkill(index);
        } else if (m_skillCount > 3) {
            freed = skill;   // no free slot
        } else {
            int n = m_skillCount;
            freed = m_skills[n];
            m_skills[n] = skill;
            m_interface.PositionSkill(m_skillCount);
            m_skillCount += 1;
            if (m_quicken) {
                skill->m_cooldown -= 1;
                if (skill->m_cooldown < skill->m_charge) skill->m_charge = skill->m_cooldown;
            }
            if (m_skillCount == 4 && Current(this).HavePerk(PERK_CUNNING)) {
                skill->m_cooldown -= 1;
                if (skill->m_cooldown < skill->m_charge) skill->m_charge = skill->m_cooldown;
            }
        }
        m_interface.UpdateSkills();
        return freed;
    }
    m_baseHealAmount = skill->m_healAmountBonus + m_baseHealAmount;
    m_baseArmourAmount = skill->m_armourAmountBonus + m_baseArmourAmount;
    auto addChance = [](int base, int bonus) {
        int v = bonus + base;
        return v > 99 ? 100 : v;
    };
    m_baseCoinTokenChance = addChance(m_baseCoinTokenChance, skill->m_coinTokenChanceBonus);
    m_basePotionTokenChance = addChance(m_basePotionTokenChance, skill->m_potionTokenChanceBonus);
    m_baseSkullTokenChance = addChance(m_baseSkullTokenChance, skill->m_skullTokenChanceBonus);
    m_baseShieldTokenChance = addChance(m_baseShieldTokenChance, skill->m_shieldTokenChanceBonus);
    if (!(m_challenge && m_mode == MODE_PRETZEL) && skill->m_maxHealthBonus > 0) {
        m_baseMaxHealth = skill->m_maxHealthBonus + m_baseMaxHealth;
        m_health = skill->m_maxHealthBonus + m_health;
        int maxHealth = m_baseMaxHealth + m_bonusMaxHealth;
        if (maxHealth != m_interface.m_maxHealth) {
            m_interface.m_maxHealth = maxHealth;
            m_interface.UpdateHealthBar(Interface::BAR_HEALTH, m_interface.m_health);
        }
        if (m_interface.m_health != m_health) {
            m_interface.m_health = m_health;
            m_interface.UpdateHealthBar(Interface::BAR_HEALTH, m_health);
        }
    }
    if (skill->m_baseDamageBonus > 0) {
        m_baseDamage = skill->m_baseDamageBonus + m_baseDamage;
        if (m_baseDamage + m_bonusDamage != m_interface.m_skullValue) {
            m_interface.m_skullValue = m_baseDamage + m_bonusDamage;
            m_interface.UpdateSkullValue();
        }
    }
    switch (static_cast<int>(skill->m_type)) {
        case 0x2a + STAT_STR: m_strength += 1; break;
        case 0x2a + STAT_DEX: m_dexterity += 1; break;
        case 0x2a + STAT_VIT: m_vitality += 1; break;
        case 0x2a + STAT_LUK: m_luck += 1; break;
        default: break;
    }
    return skill;
}

// Moves the skill to the end of the used slots and shrinks the count (the object stays owned).
void Game::RemoveSkill(Skill* skill) {
    int count = m_skillCount;
    if (count < 1) return;
    int index = 0;
    while (m_skills[index] != skill) {
        if (++index == count) return;
    }
    int last = count - 1;
    while (index < last) {
        m_skills[index] = m_skills[index + 1];
        m_interface.PositionSkill(index);
        ++index;
        last = m_skillCount - 1;
    }
    m_skills[last] = skill;
    m_skillCount -= 1;
}

bool Game::HasSkill(int type) {
    for (int i = 0; i < m_skillCount; ++i) {
        if (m_skills[i]->m_type == type) return true;
    }
    return false;
}

namespace {
// Adds `count` units plus the bonus rate's fraction; whole units of the fraction move into the
// value (`fraction` keeps the rest).
int AddWithFraction(int value, int units, float& fraction, float rate, float multiplier, bool useMultiplier) {
    float f = static_cast<float>(units);
    float total = useMultiplier ? fraction + (f * rate) * multiplier : fraction + f * rate;
    fraction = total;
    if (!(total < 1.0f) && total == total) {
        int whole = FloatToIntArm(total);
        value += whole;
        fraction = total - static_cast<float>(whole);
    }
    return value;
}
}  // namespace

// A collected (or attacking) chain resource. tokenType: 1 = potion on a skill button (charges the
// skill), 2 = acid potion (damage), 3 = enemy damage (amount); amounts per token double in a
// sprint challenge.
void Game::AddResource(TileType type, int tokenType, int amount) {
    int units = (m_challenge && m_mode == MODE_SPRINT) ? 2 : 1;
    switch (type) {
        case TILE_POTION: {
            if (amount >= 0 && tokenType == 1) {
                if (amount < m_skillCount) {
                    QueueSound(SOUND_POTION);
                    Skill* s = m_skills[amount];
                    int charge = s->m_charge + 1;
                    s->m_charge = s->m_cooldown < charge ? s->m_cooldown : charge;
                    m_interface.UpdateSkills();
                    return;
                }
            } else if (tokenType == 2) {
                QueueSound(SOUND_DAMAGE);
                m_health = (-m_bonusHealAmount - m_baseHealAmount) + m_health;
                if (m_health < 0) {
                    m_health = 0;
                    QueueAchievement(ACH_DISINTEGRATED, 100.0f);
                }
                m_interface.SetHealth(m_health);
                CheckEndGame();
                return;
            }
            QueueSound(SOUND_POTION);
            int health = (m_bonusHealAmount + m_baseHealAmount) * (m_doubleHealth ? 2 : 1) + m_health;
            int maxHealth = m_bonusMaxHealth + m_baseMaxHealth;
            m_health = health;
            if (maxHealth < health) {
                m_health = maxHealth;
                health = maxHealth;
            }
            if (health != m_interface.m_health) {
                m_interface.m_health = health;
                m_interface.UpdateHealthBar(Interface::BAR_HEALTH, health);
            }
            if (CharacterClass::GetMortalEnemyType(Current(this).m_race) != ENEMY_POISONOUS ||
                GetEnemyCount(ENEMY_POISONOUS) == 0) {
                m_poison = 0;
                m_interface.SetPoison(0);
            }
            break;
        }
        case TILE_SKULL: {
            if (tokenType == 3) {
                QueueSound(SOUND_DAMAGE);
                int health = m_health - amount;
                m_health = health < 0 ? 0 : health;
                m_interface.SetHealth(m_health);
                AddSimpleLabel(2, m_interface.GetTarget(TILE_POTION), amount, 0xb, 0.5f);
                if (m_lastStandActivePrevTurn && !m_lastStandActive) {
                    m_lastStandActive = true;
                    CheckEndGame();
                    m_lastStandActive = false;
                    return;
                }
                CheckEndGame();
                return;
            }
            QueueSound(SOUND_XP);
            m_interface.ShowHelpXP();
            int base = m_xp + units * 2;
            m_xp = base;
            int xp = AddWithFraction(base, units, m_xpFraction, m_xpBonusRate, 1.0f, false);
            m_xp = xp;
            if (xp > 99) {
                m_interface.SetXPshadow(-1);
                m_xpBanked += 100;
                m_level += 1;
                m_interface.ShowMenu(0);
                m_xp = m_xp % 100;
                if (Current(this).m_race == RACE_FAUN) Healing();
                switch (m_level) {
                    case 5:
                        QueueAchievement(ACH_CURIOUS, 100.0f);
                        if (m_difficulty == DIFF_HARDER && m_skillCount == 0) QueueAchievement(ACH_PIECE_OF_CAKE, 100.0f);
                        break;
                    case 10:
                        QueueAchievement(ACH_SKILLED, 100.0f);
                        if (m_difficulty == DIFF_HARDER && Current(this).GetPerkCount() == 1 &&
                            Current(this).GetFlawCount() == 4) {
                            QueueAchievement(ACH_TRAGIC_HERO, 100.0f);
                        }
                        break;
                    case 25: QueueAchievement(ACH_HEROIC, 100.0f); break;
                    case 50: QueueAchievement(ACH_LEGENDARY, 100.0f); break;
                    case 100: QueueAchievement(ACH_ASCENDANT, 100.0f); break;
                    default: break;
                }
                xp = m_xp;
            }
            if (xp != m_interface.m_xp) {
                m_interface.m_xp = xp;
                m_interface.UpdateXPBar(Interface::BAR_XP, xp);
            }
            break;
        }
        case TILE_COIN: {
            QueueSound(SOUND_COIN);
            m_interface.ShowHelpGold();
            int base = (m_doubleGold ? 2 : 1) * units * 2 + m_gold;
            m_gold = base;
            int gold = AddWithFraction(base, units, m_goldFraction, m_goldBonusRate, m_doubleGold ? 2.0f : 1.0f, true);
            m_gold = gold;
            if (gold > 99) {
                m_interface.SetGoldShadow(-1);
                m_goldBanked += 100;
                m_interface.ShowMenu(1);
                gold = m_gold % 100;
                m_gold = gold;
            }
            if (gold != m_interface.m_gold) {
                m_interface.m_gold = gold;
                m_interface.UpdateGoldBar(Interface::BAR_GOLD, gold);
            }
            break;
        }
        case TILE_SHIELD: {
            if (m_armour == m_maxArmour || GetEnemyCount(ENEMY_CORROSIVE) != 0) {
                // full armour (or a Corrosive enemy): upgrade points instead
                QueueSound(SOUND_UP);
                m_interface.ShowHelpUP();
                int base = (m_doubleArmour ? 2 : 1) * units * 2 + m_upgradePoints;
                m_upgradePoints = base;
                int up = AddWithFraction(base, units, m_upgradeFraction, m_upgradeBonusRate,
                                         m_doubleArmour ? 2.0f : 1.0f, true);
                m_upgradePoints = up;
                if (up > 99) {
                    m_interface.SetUPshadow(-1);
                    m_upgradePointsBanked += 100;
                    m_interface.ShowMenu(2);
                    up = m_upgradePoints % 100;
                    m_upgradePoints = up;
                }
                if (up != m_interface.m_upgradePoints) {
                    m_interface.m_upgradePoints = up;
                    m_interface.UpdateUPBar(Interface::BAR_UPGRADE, up);
                }
            } else {
                QueueSound(SOUND_SHIELD);
                m_interface.ShowHelpArmour();
                int armour = (m_bonusArmourAmount + m_baseArmourAmount) * (m_doubleArmour ? 2 : 1) + m_armour;
                m_armour = armour;
                if (m_maxArmour < armour) {
                    m_armour = m_maxArmour;
                    armour = m_maxArmour;
                }
                if (armour != m_interface.m_armour) {
                    m_interface.m_armour = armour;
                    m_interface.UpdateShieldValue();
                }
            }
            break;
        }
        default:
            break;
    }
}

// Health below a quarter of the maximum is raised to it (Faun level-ups).
void Game::Healing() {
    float quarter = static_cast<float>(m_bonusMaxHealth + m_baseMaxHealth) * 0.25f;
    if (static_cast<float>(m_health) < quarter) {
        int health = FloatToIntArm(quarter);
        QueueAchievement(ACH_AINT_DEAD_YET, 5.0f);
        m_health = health;
        if (health != m_interface.m_health) {
            m_interface.m_health = health;
            m_interface.UpdateHealthBar(Interface::BAR_HEALTH, health);
        }
    }
}

// Normal: all resources + turn + 100 per skill level. Pretzel: its own score. Sprint: a quarter of
// each resource total (doubled after turn 100); no turn or skill bonus.
int Game::GetScore() {
    int score;
    if (!m_challenge) {
        score = (m_goldBanked + m_xpBanked + m_xp + m_gold) + (m_upgradePointsBanked + m_upgradePoints);
    } else if (m_mode == MODE_PRETZEL) {
        return m_pretzelScore;
    } else if (m_mode == MODE_SPRINT) {
        int s = (m_xp + m_xpBanked) / 4 + (m_gold + m_goldBanked) / 4 + (m_upgradePoints + m_upgradePointsBanked) / 4;
        return m_reachedTurn100 ? s * 2 : s;
    } else {
        score = (m_gold + m_goldBanked + m_xp + m_xpBanked) + (m_upgradePoints + m_upgradePointsBanked);
    }
    score = score + m_turn;
    for (int i = 0; i < m_skillCount; ++i) score = m_skills[i]->m_level * 100 + score;
    return score;
}

// ================================================================ classes, achievements, sounds

// Skills from 31 on are unlocked by getting the class whose first base skill they are to level 10.
bool Game::IsSkillUnlocked(SkillType skill) {
    if (static_cast<int>(skill) < 31) return true;
    for (int c = 1;; ++c) {
        if (skill == m_classes[c].m_baseSkills[0]) return m_classes[c].m_level == 10;
        if (c + 1 > 9) return false;
    }
}

bool Game::IsPerkUnlocked(PerkType perk) {
    if (perk == PERK_UNEXPECTED) return true;
    for (int c = 0; c < 10; ++c) {
        if (perk == m_classes[c].m_perks[0]) return m_classes[c].IsPerkUnlocked(0);
    }
    return false;
}

bool Game::IsRaceUnlocked(RaceType race) {
    for (int c = 0; c < 10; ++c) {
        if (race == m_classes[c].m_nativeRace) {
            if (m_classes[c].m_class != CLASS_ADVENTURER) return m_classes[c].m_level > 4;
            return true;
        }
    }
    return false;
}

void Game::SetClassLevel(ClassType type, int level) {
    if (static_cast<int>(m_classes[type].m_level) < level) m_classes[type].SetLevel(level);
}

// All classes but the Adventurer at level 10.
void Game::CheckIfAllClassesMaxed() {
    for (int c = 1; c < 10; ++c) {
        if (m_classes[c].m_level < 10) return;
    }
    QueueAchievement(ACH_CLASSY, 100.0f);
}

// Queued for the Java shell (OpenFeint). Easy difficulty earns nothing but Mission Accomplished.
void Game::QueueAchievement(Achievement achievement, float progress) {
    if (m_difficulty == DIFF_EASY && achievement != ACH_MISSION_ACCOMPLISHED) return;
    QueuedAchievement entry = {achievement, Clamp(progress, 0.0f, 100.0f)};
    if (static_cast<size_t>(m_achievementCount) < m_achievementQueue.size()) {
        m_achievementQueue[m_achievementCount] = entry;
    } else {
        m_achievementQueue.push_back(entry);
    }
    m_achievementCount += 1;
}

Achievement Game::GetNextAchievement(float& progress) {
    if (m_achievementRead < m_achievementCount) {
        const QueuedAchievement& e = m_achievementQueue[m_achievementRead];
        progress = e.progress;
        m_achievementRead += 1;
        return e.achievement;
    }
    return static_cast<Achievement>(0);
}

void Game::QueueSound(Sound sound) {
    if (!m_soundEnabled) return;
    if (static_cast<size_t>(static_cast<unsigned>(m_soundCount)) < m_soundQueue.size()) {
        m_soundQueue[m_soundCount] = sound;
    } else {
        m_soundQueue.push_back(sound);
    }
    m_soundCount += 1;
}

int Game::GetNextSound() {
    if (m_soundRead < m_soundCount) return m_soundQueue[m_soundRead++];
    return -1;
}

// ================================================================ new game, save states

void Game::InitNewGame() {
    SetRandSeed(GetUnixTime(), RAND_TYPE_COUNT);
    FlushRandSeeds();
    CalculateTilePositions();
    for (CharacterClass& c : m_classes) c.UnlockAchievement();
    m_reachedTurn100 = false;
    m_pretzelScore = 0;
    m_scoreGain = 0;
    m_scoreBonus = 0;
    m_chainCount = 0;
    m_averageChainLength10 = 0;
    m_longestChain = 0;
    m_chainPotionTokens = 0;
    m_chainBonusPotionTokens = 0;
    m_chainSkullTokens = 0;
    m_chainBonusSkullTokens = 0;
    m_chainCoinTokens = 0;
    m_chainBonusCoinTokens = 0;
    m_chainShieldTokens = 0;
    m_chainBonusShieldTokens = 0;
    m_chainSwordTokens = 0;
    m_chainBonusSwordTokens = 0;
    m_turn = 0;
    m_chainDamage = 0;
    m_lifeLeechHeal = 0;
    m_chainBurnDamage = 0;
    m_spikyDamage = 0;
    m_trophiesSpawned = 0;
    m_enemiesKilled = 0;
    m_poison = 0;
    m_crossLabelCount = 0;
    m_promotionTimerEnabled = true;
    if (!m_challenge || m_mode != MODE_SPRINT) {
        m_timeToPromotion = Current(this).HaveFlaw(FLAW_ZEALOUS) ? 49 : 50;
    } else {
        m_timeToPromotion = 200;
    }
    m_hackUsedThisTurn = false;
    m_slashUsedThisTurn = false;
    m_collectingBoard = false;
    m_runeActive = false;
    m_runeActiveNextTurn = false;
    m_lastStandActive = false;
    m_lastStandActiveNextTurn = false;
    m_lastStandActivePrevTurn = false;
    m_berserkActive = false;
    m_berserkActiveNextTurn = false;
    m_chainHasSpiky = false;
    m_keepChainBurnDamage = 0;
    m_keepSpikyDamage = false;
    m_fillingNewBoard = true;
    m_counterActive = false;
    m_stealActive = false;
    m_stealActiveNextTurn = false;
    m_trapActive = false;
    m_treasureChamberActive = false;
    m_trapActiveNextTurn = false;
    m_treasureChamberActiveNextTurn = false;
    m_doubleDamage = m_doubleArmour = m_doubleHealth = m_doubleGold = false;
    m_doubleDamageNextTurn = m_doubleArmourNextTurn = m_doubleHealthNextTurn = m_doubleGoldNextTurn = false;
    m_fpsLabel.SetText("fps: ?", -1);
    m_fpsLabel.m_scale = {0.1f, 0.1f};
    m_fpsLabel.m_position = {0.8f, 0.05f};
    m_fpsLabel.m_colour = {0xff, 0xff, 0xff, 0xff};
    m_fpsLabel.UpdateColour();
    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 6; ++col) {
            Tile* t = &m_board[row][col];
            PositionTile(t, col, row, false);
            RandomiseTile(t);
        }
    }
    m_fillingNewBoard = false;
    m_newBoardAvoidsSwords = false;
    m_strength = m_dexterity = m_vitality = m_luck = 1;
    m_baseDamage = 3;
    if (Current(this).HavePerk(PERK_STRONG)) m_baseDamage += 1;
    m_baseMaxHealth = 40;
    if (m_challenge && m_mode == MODE_PRETZEL) m_baseMaxHealth = 100;
    if (Current(this).HavePerk(PERK_TOUGH)) m_baseMaxHealth += 10;
    if (Current(this).HaveFlaw(FLAW_FRAIL)) m_baseMaxHealth -= 10;
    if (m_baseDamage != m_interface.m_skullValue) {
        m_interface.m_skullValue = m_baseDamage;
        m_interface.UpdateSkullValue();
    }
    int health = m_interface.m_maxHealth;
    if (m_baseMaxHealth != health) {
        m_interface.m_maxHealth = m_baseMaxHealth;
        m_interface.UpdateHealthBar(Interface::BAR_HEALTH, m_interface.m_health);
        health = m_baseMaxHealth;
    }
    m_health = health;
    if (m_interface.m_health != health) {
        m_interface.m_health = health;
        m_interface.UpdateHealthBar(Interface::BAR_HEALTH, health);
    }
    m_xp = 0;
    if (m_interface.m_xp != 0) {
        m_interface.m_xp = 0;
        m_interface.UpdateXPBar(Interface::BAR_XP, 0);
    }
    m_upgradePoints = 0;
    if (m_interface.m_upgradePoints != 0) {
        m_interface.m_upgradePoints = 0;
        m_interface.UpdateUPBar(Interface::BAR_UPGRADE, 0);
    }
    m_gold = 0;
    if (m_interface.m_gold != 0) {
        m_interface.m_gold = 0;
        m_interface.UpdateGoldBar(Interface::BAR_GOLD, 0);
    }
    m_baseArmourAmount = 1;
    m_baseHealAmount = 1;
    m_baseCoinTokenChance = m_basePotionTokenChance = m_baseSkullTokenChance = m_baseShieldTokenChance = 20;
    if (Current(this).HavePerk(PERK_FRUGAL)) m_baseCoinTokenChance += 10;
    if (Current(this).HaveFlaw(FLAW_RECKLESS)) m_baseShieldTokenChance -= 10;
    m_armour = 0;
    m_maxArmour = 0;
    for (Item*& item : m_items) {
        delete item;
    }
    m_items[0] = new Item(this, WEAPON);
    m_items[1] = new Item(this, HELMET);
    m_items[2] = new Item(this, ARMOUR);
    m_items[3] = new Item(this, SHIELD);
    m_items[4] = new Item(this, ACCESSORY);
    for (Skill* s : m_skills) {
        for (QuadVertex& v : s->m_iconVertices) v.colour.a = 0xff;
    }
    m_bonusMaxHealth = 0;
    m_quicken = false;
    UpdateStats();
    m_xpFraction = 0.0f;
    m_goldFraction = 0.0f;
    m_upgradeFraction = 0.0f;
    m_attackPhase = 0;
    m_inputReady = 1;
    m_shieldShatterActive = false;
    m_skillCount = 0;
    m_fallingTiles.clear();
    m_tokenCount = 0;
    m_particleCount = 0;
    m_simpleLabelCount = 0;
    m_level = 1;
    m_xpBanked = 0;
    m_goldBanked = 0;
    m_upgradePointsBanked = 0;
    m_chainMadeThisTurn = false;
    m_simpleLabelC388.m_active = false;
    m_simpleLabel14344.m_active = false;
    m_simpleLabel146b8.m_active = false;
    m_incomingPoisonLabel.m_active = false;
    m_interface.Reset();
}

// Format 3: 0xff, 3, challenge mode (0xff = none), pretzel score, chain statistics, the class,
// trophies, the 36 tile types (two per byte), every tile, turn, kills, base chances, poison, 0,
// promotion timer, attributes and base stats, health, the resource totals, armour, the items, the
// skills, the name, five RNG seeds, the help pages shown, the difficulty.
void Game::SerialiseState() {
    GameState* s = &m_saveState;
    s->ResetForWriting();
    s->WriteChar(0xff);
    s->WriteChar(3);
    s->WriteChar(m_challenge ? static_cast<uint8_t>(m_mode) : 0xff);
    s->WriteInteger(m_pretzelScore);
    s->WriteInteger(m_chainCount);
    s->WriteChar(static_cast<uint8_t>(m_averageChainLength10));
    s->WriteChar(static_cast<uint8_t>(m_longestChain));
    s->WriteChar(static_cast<uint8_t>(m_currentClass));
    m_classes[m_currentClass].SerialiseGameState(s);
    s->WriteChar(static_cast<uint8_t>(m_trophiesSpawned));
    bool high = true;
    unsigned packed = 0;
    for (auto& row : m_board) {
        for (Tile& t : row) {
            if (high) {
                packed = (static_cast<unsigned>(t.m_type) & 0xf) << 4;
            } else {
                packed = packed | (static_cast<unsigned>(t.m_type) & 0xff);
                s->WriteChar(static_cast<uint8_t>(packed));
            }
            high = !high;
        }
    }
    for (auto& row : m_board) {
        for (Tile& t : row) t.Serialise(s);
    }
    s->WriteInteger(m_turn);
    s->WriteInteger(m_enemiesKilled);
    s->WriteChar(static_cast<uint8_t>(m_baseCoinTokenChance));
    s->WriteChar(static_cast<uint8_t>(m_basePotionTokenChance));
    s->WriteChar(static_cast<uint8_t>(m_baseSkullTokenChance));
    s->WriteChar(static_cast<uint8_t>(m_baseShieldTokenChance));
    s->WriteChar(static_cast<uint8_t>(m_poison));
    s->WriteChar(0);
    s->WriteChar(static_cast<uint8_t>(m_timeToPromotion));
    s->WriteInteger(m_strength);
    s->WriteInteger(m_dexterity);
    s->WriteInteger(m_vitality);
    s->WriteInteger(m_luck);
    s->WriteInteger(m_baseDamage);
    s->WriteInteger(m_baseArmourAmount);
    s->WriteInteger(m_baseHealAmount);
    s->WriteInteger(m_baseMaxHealth);
    s->WriteInteger(m_health);
    s->WriteInteger(m_xp + m_xpBanked);
    s->WriteInteger(m_upgradePoints + m_upgradePointsBanked);
    s->WriteInteger(m_gold + m_goldBanked);
    s->WriteInteger(m_armour);
    for (Item** slot : m_itemSlots) (*slot)->Serialise(s);
    s->WriteChar(static_cast<uint8_t>(m_skillCount));
    for (int i = 0; i < m_skillCount; ++i) m_skills[i]->Serialise(s);
    for (int i = 0; i < 17; ++i) {
        s->WriteChar(static_cast<uint8_t>(m_name[i]));
        if (m_name[i] == '\0') break;
    }
    s->WriteInteger(static_cast<int>(GetRandSeed(RAND_TILE)));
    s->WriteInteger(static_cast<int>(GetRandSeed(RAND_ITEM)));
    s->WriteInteger(static_cast<int>(GetRandSeed(RAND_SKILL)));
    s->WriteInteger(static_cast<int>(GetRandSeed(RAND_UPGRADE)));
    s->WriteInteger(static_cast<int>(GetRandSeed(RAND_OTHER)));
    m_helpMenu.Serialise(s);
    s->WriteChar(static_cast<uint8_t>(m_difficulty));
}

// Format by the first byte: 0xff = format 2/3 (second byte: version), else format 1.
void Game::DeserialiseState() {
    GameState* s = &m_saveState;
    s->ResetForReading();
    if (s->ReadChar() == 0xff) {
        int version = s->ReadChar();
        if (version == 0) {
            DeserialiseState2(s, 0, 0);
        } else {
            DeserialiseState2(s, 1, version);
        }
    } else {
        DeserialiseState1();
    }
}

namespace {
// The values the HUD shows after loading (shared tail of both formats).
void SyncInterfaceAfterLoad(Game* g) {
    Interface& ui = g->m_interface;
    int skull = g->m_bonusDamage + g->m_baseDamage;
    if (skull != ui.m_skullValue) {
        ui.m_skullValue = skull;
        ui.UpdateSkullValue();
    }
    int maxHealth = g->m_bonusMaxHealth + g->m_baseMaxHealth;
    if (maxHealth != ui.m_maxHealth) {
        ui.m_maxHealth = maxHealth;
        ui.UpdateHealthBar(Interface::BAR_HEALTH, ui.m_health);
    }
    if (g->m_health != ui.m_health) {
        ui.m_health = g->m_health;
        ui.UpdateHealthBar(Interface::BAR_HEALTH, g->m_health);
    }
    if (g->m_maxArmour != ui.m_maxArmour) {
        ui.m_maxArmour = g->m_maxArmour;
        ui.UpdateShieldValue();
    }
    if (g->m_armour != ui.m_armour) {
        ui.m_armour = g->m_armour;
        ui.UpdateShieldValue();
    }
    if (g->m_xp != ui.m_xp) {
        ui.m_xp = g->m_xp;
        ui.UpdateXPBar(Interface::BAR_XP, g->m_xp);
    }
    if (g->m_upgradePoints != ui.m_upgradePoints) {
        ui.m_upgradePoints = g->m_upgradePoints;
        ui.UpdateUPBar(Interface::BAR_UPGRADE, g->m_upgradePoints);
    }
    if (g->m_gold != ui.m_gold) {
        ui.m_gold = g->m_gold;
        ui.UpdateGoldBar(Interface::BAR_GOLD, g->m_gold);
    }
}

// The common end of both formats: skills, name, RNG seeds, help pages, difficulty.
void ReadStateTail(Game* g, GameState* s) {
    g->m_skillCount = s->ReadChar();
    for (int i = 0; i < g->m_skillCount; ++i) {
        g->m_skills[i]->Deserialise(s);
        g->m_interface.PositionSkill(i);
    }
    g->m_interface.UpdateSkills();
    for (int i = 0; i < 17; ++i) {
        int c = s->ReadChar();
        g->m_name[i] = static_cast<char>(c);
        if (c == 0) break;
    }
    SetRandSeed(static_cast<uint32_t>(s->ReadInteger()), RAND_TILE);
    SetRandSeed(static_cast<uint32_t>(s->ReadInteger()), RAND_ITEM);
    SetRandSeed(static_cast<uint32_t>(s->ReadInteger()), RAND_SKILL);
    SetRandSeed(static_cast<uint32_t>(s->ReadInteger()), RAND_UPGRADE);
    SetRandSeed(static_cast<uint32_t>(s->ReadInteger()), RAND_OTHER);
    g->m_helpMenu.Deserialise(s);
    g->SetHelpState(g->m_helpMenu.m_shownHelp != 0);
    int difficulty = s->ReadChar();
    Difficulty old = g->m_difficulty;
    g->m_difficultyChanged = old != static_cast<Difficulty>(ClampI(difficulty, 0, 3));
    g->m_difficulty = static_cast<Difficulty>(ClampI(difficulty, 0, 3));
}

// Splits a saved total into the 0..99 progress and the banked hundreds.
void SplitBanked(int& value, int& banked) {
    value = banked % 100;
    banked = banked - value;
}
}  // namespace

// The oldest format: tile types as nibbles (5 = explosive potion; one byte more is read after the
// last tile), old tile data, one-byte stats.
void Game::DeserialiseState1() {
    GameState* s = &m_saveState;
    int byte = s->ReadChar();
    bool high = true;
    for (auto& row : m_board) {
        for (Tile& t : row) {
            int type = high ? byte >> 4 : byte & 0xf;
            if (type == TILE_TYPE_COUNT) {
                t.SetType(TILE_POTION, 0, m_difficulty);
                t.MakeExplosivePotion();
            } else {
                t.SetType(static_cast<TileType>(type), 0, m_difficulty);
            }
            if (!high) byte = s->ReadChar();
            high = !high;
        }
    }
    for (auto& row : m_board) {
        for (Tile& t : row) t.Deserialise1(s);
    }
    m_turn = s->ReadInteger();
    m_strength = s->ReadChar();
    m_dexterity = s->ReadChar();
    m_vitality = s->ReadChar();
    m_luck = s->ReadChar();
    m_baseDamage = s->ReadChar();
    m_baseArmourAmount = s->ReadChar();
    m_baseHealAmount = s->ReadChar();
    m_baseCoinTokenChance = s->ReadChar();
    m_basePotionTokenChance = s->ReadChar();
    m_baseSkullTokenChance = s->ReadChar();
    m_baseShieldTokenChance = s->ReadChar();
    m_armour = s->ReadChar();
    m_baseMaxHealth = s->ReadInteger();
    m_health = s->ReadInteger();
    m_xpBanked = s->ReadInteger();
    m_upgradePointsBanked = s->ReadInteger();
    m_goldBanked = s->ReadInteger();
    SplitBanked(m_xp, m_xpBanked);
    SplitBanked(m_upgradePoints, m_upgradePointsBanked);
    SplitBanked(m_gold, m_goldBanked);
    m_level = m_xpBanked / 100 + 1;
    for (Item** slot : m_itemSlots) (*slot)->Deserialise1(s);
    int armour = m_armour;
    int health = m_health;
    m_armour = 0;
    m_maxArmour = 0;
    m_bonusMaxHealth = 0;
    UpdateStats();
    m_armour = armour;
    m_health = health;
    SyncInterfaceAfterLoad(this);
    ReadStateTail(this, s);
}

// Formats 2 (version < 3: no challenge/statistics; version 1 also no class) and 3.
void Game::DeserialiseState2(GameState* s, int tileVersion, int version) {
    m_trophiesSpawned = 0;
    bool readClass = true;
    if (version < 3) {
        m_pretzelScore = 0;
        m_challenge = false;
        if (version != 2) {
            m_currentClass = 0;
            readClass = false;
        }
    } else {
        int mode = s->ReadChar();
        m_challenge = mode != 0xff;
        if (mode != 0xff) m_mode = static_cast<GameMode>(ClampI(mode, 0, 1));
        m_pretzelScore = s->ReadInteger();
        m_chainCount = s->ReadInteger();
        m_averageChainLength10 = s->ReadChar();
        m_longestChain = s->ReadChar();
    }
    if (readClass) {
        int cls = s->ReadChar();
        m_currentClass = cls;
        m_classes[cls].DeserialiseGameState(s);
        m_trophiesSpawned = s->ReadChar();
    }
    m_timeToPromotion = 1;
    bool high = true;
    int byte = 0;
    for (auto& row : m_board) {
        for (Tile& t : row) {
            int type;
            if (high) {
                byte = s->ReadChar();
                type = byte >> 4;
            } else {
                type = byte & 0xf;
            }
            high = !high;
            t.SetType(static_cast<TileType>(type), 0, m_difficulty);
        }
    }
    for (auto& row : m_board) {
        for (Tile& t : row) t.Deserialise2(s, tileVersion);
    }
    m_turn = s->ReadInteger();
    if (version > 1) m_enemiesKilled = s->ReadInteger();
    m_baseCoinTokenChance = s->ReadChar();
    m_basePotionTokenChance = s->ReadChar();
    m_baseSkullTokenChance = s->ReadChar();
    m_baseShieldTokenChance = s->ReadChar();
    m_poison = s->ReadChar();
    s->ReadChar();
    m_timeToPromotion = s->ReadChar();
    m_strength = s->ReadInteger();
    m_dexterity = s->ReadInteger();
    m_vitality = s->ReadInteger();
    m_luck = s->ReadInteger();
    m_baseDamage = s->ReadInteger();
    m_baseArmourAmount = s->ReadInteger();
    m_baseHealAmount = s->ReadInteger();
    m_baseMaxHealth = s->ReadInteger();
    m_health = s->ReadInteger();
    m_xpBanked = s->ReadInteger();
    m_upgradePointsBanked = s->ReadInteger();
    m_goldBanked = s->ReadInteger();
    m_armour = s->ReadInteger();
    SplitBanked(m_xp, m_xpBanked);
    SplitBanked(m_upgradePoints, m_upgradePointsBanked);
    SplitBanked(m_gold, m_goldBanked);
    m_level = m_xpBanked / 100 + 1;
    int armour = m_armour;   // (the original takes it only when there are items; there always are)
    for (Item** slot : m_itemSlots) (*slot)->Deserialise2(s);
    int health = m_health;
    m_armour = 0;
    m_maxArmour = 0;
    m_bonusMaxHealth = 0;
    UpdateStats();
    m_armour = armour;
    m_health = health;
    SyncInterfaceAfterLoad(this);
    m_interface.SetPoison(m_poison);
    ReadStateTail(this, s);
}

// 0, 10 (= class count), then every class's full state.
void Game::SerialiseClasses() {
    GameState* s = &m_classState;
    s->ResetForWriting();
    s->WriteChar(0);
    s->WriteChar(10);
    for (CharacterClass& c : m_classes) c.SerialiseFullState(s);
}

void Game::DeserialiseClasses() {
    GameState* s = &m_classState;
    s->ReadChar();
    if (s->ReadChar() == 10) {
        for (CharacterClass& c : m_classes) c.DeserialiseFullState(s);
    }
}

// ================================================================ settings

const char* Game::GetDifficultyName(int difficulty) {
    switch (difficulty) {
        case 0: return "Easy";
        case 1: return "Normal";
        case 2: return "Hard";
        case 3: return "Harder";
        default: return "";
    }
}

const char* Game::GetModeName(int mode) {
    if (mode == 1) return "Pretzel Hero";
    if (mode == 3) return "Classic";
    if (mode == 0) return "Dungeon Sprint";
    return "";
}

void Game::SetSoundState(bool enabled) {
    m_soundStateChanged = m_soundEnabled != enabled;
    m_soundEnabled = enabled;
    m_interface.m_gameMenu.SetSoundState(enabled);
    m_mainMenu.SetSoundState(enabled);
}

// Port-only: the settings screen's volume slider. 0 turns the sound off (the original's flag, which
// QueueSound checks), anything else on; the host applies the level to its mixer and saves it.
void Game::SetSoundVolume(int percent) {
    percent = ClampI(percent, 0, 100);
    m_soundVolumeChanged = m_soundVolumeChanged || percent != m_soundVolume;
    m_soundVolume = percent;
    SetSoundState(percent > 0);
}

// None: the original 2:3 screen (and its tooltip bubbles); Top / Bottom: the tall canvas, the panel
// above or below the board; Horizontal (desktop only, else Bottom): the horizontal canvas.
void Game::SetPanelMode(int mode) {
    mode = ClampI(mode, PANEL_NONE, m_desktop ? PANEL_HORIZONTAL : PANEL_BOTTOM);
    m_panelModeChanged = m_panelModeChanged || mode != m_panelMode;
    m_panelMode = mode;
    m_tallCanvas = mode != PANEL_NONE;
    m_panelAtBottom = mode == PANEL_BOTTOM;
    m_horizontal = mode == PANEL_HORIZONTAL;
    SetupRenderer();   // the viewport's shape (desktop hosts fit the window to it)
}

void Game::SetGrind(int grind) {
    grind = ClampI(grind, 0, 2);
    m_grindChanged = m_grindChanged || grind != m_grind;
    m_grind = grind;
}

void Game::SetAltSelection(bool on) {
    m_altSelectionChanged = m_altSelectionChanged || on != m_altSelection;
    m_altSelection = on;
}

void Game::SetHelpState(bool enabled) {
    m_helpEnabled = enabled;
    m_interface.m_gameMenu.SetHelpState(enabled);
}

void Game::SetClockState(bool enabled) {
    m_clockStateChanged = m_clockEnabled != enabled;
    m_clockEnabled = enabled;
    m_interface.SetClockState(enabled);
}

void Game::SetAttackDimmingState(bool enabled) {
    m_dimmingStateChanged = m_attackDimmingEnabled != enabled;
    m_attackDimmingEnabled = enabled;
    m_interface.m_gameMenu.SetDimmingState(enabled);
}

void Game::SetHighFramerate(bool enabled) {
    m_highFramerateStateChanged = m_highFramerate != enabled;
    m_highFramerate = enabled;
    m_interface.m_gameMenu.SetSmoothState(enabled);
}

// Help pages switched on: shown again from the start; switched off: the pending one dropped.
void Game::UpdateHelpState() {
    if (m_helpMenu.m_shownHelp == 0) {
        if (m_helpEnabled) m_helpMenu.ResetHelp();
    } else if (!m_helpEnabled) {
        m_helpMenu.ClearHelp();
    }
}

void Game::ShowBonusRequest() {
    m_unlockMenu.ShowPromo("Unlock ability: Hack", "Unlock promotional ability Hack by downloading the FREE PlayHaven App!");
}

// After a game with 300+ experience (a quarter in Sprint): counts toward the nag screen, shown at
// the tenth.
void Game::ShowNagScreen() {
    int xp = m_xp + m_xpBanked;
    if (m_challenge && m_mode == MODE_SPRINT) xp /= 4;
    if (xp <= 299) return;
    int n = ++m_nagScreenCount;
    if (n < 11) {
        m_nagScreenCountChanged = true;
        if (n == 10) m_showNagScreen = true;
    }
}
