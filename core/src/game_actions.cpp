// Game: the board actions of the skills (Skill::Activate) and of the special enemies
// (Tile::ActivateAbility), slice 2a. See game.h.
#include "game.h"

#include <cmath>
#include <cstdlib>
#include <set>
#include <vector>

#include "item.h"
#include "rand.h"
#include "skill.h"


namespace {
const GLcolour4 kWhite = {0xff, 0xff, 0xff, 0xff};
const GLcolour4 kBlueFlare = {100, 100, 0xff, 0xff};
const GLcolour4 kFireFlare = {0xf0, 0x96, 10, 0xff};
const GLcolour4 kEarthFlare = {115, 60, 20, 0xff};
const GLcolour4 kGreenFlare = {0, 0xff, 0, 0xff};
const GLcolour4 kYellowFlare = {0xff, 0xff, 0, 0xff};

bool IsNormalPotion(const Tile& t) {
    return t.m_type == TILE_POTION && !t.m_explosivePotion && !t.m_manaPotion && !t.m_acidPotion;
}
bool IsNormalSword(const Tile& t) { return t.m_type == TILE_SWORD && !t.m_magicSword && !t.m_arrow; }
bool IsSpecialEnemy(const Tile& t) { return t.m_type == TILE_SKULL && t.m_enemyType != ENEMY_NORMAL; }
bool IsNormalEnemy(const Tile& t) { return t.m_type == TILE_SKULL && t.m_enemyType == ENEMY_NORMAL; }
}  // namespace

// After most actions the board flashes: 0xc150 = 0, 0xc154 = 1.
void Game::StartBoardFlash() {
    m_boardFlashTime = 0.0f;
    m_boardFlashLength = 1.0f;
}

// Picks the index-th tile (column by column, top to bottom) that satisfies `pred`, with index =
// RandI(0, count - 1, RAND_OTHER) drawn by the caller; returns null when index < 0 or none.
template <typename Pred>
Tile* Game::PickTile(int index, Pred pred, int* rowOut, int* colOut) {
    if (index < 0) return nullptr;
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            Tile& t = m_board[row][col];
            if (pred(t)) {
                if (index == 0) {
                    *rowOut = row;
                    *colOut = col;
                    return &t;
                }
                --index;
            }
        }
    }
    return nullptr;
}

void Game::Upgrade() {
    m_interface.ShowMenu(2);
}

void Game::MimicDefence(Tile* tile) {
    tile->m_armour = m_armour;
    tile->UpdateText(false, true, false);
    AddSimpleLabel(0x14, &tile->m_position, 0, 0x1c, 1.0f);
    QueueSound(SOUND_BOOST);
}

// Every tile swaps with a random one (column by column).
void Game::Earthquake() {
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            int c2 = RandI(0, 5, RAND_OTHER);
            int r2 = RandI(0, 5, RAND_OTHER);
            SwapTiles(col, row, c2, r2);
            AddFlare(1, m_gridPositions[row][col], kEarthFlare);
        }
    }
    StartBoardFlash();
}

// A chain tile on a random cell that is not a special enemy (10 tries).
void Game::CreateChain() {
    for (int i = 0; i < 10; ++i) {
        int col = RandI(0, 5, RAND_OTHER);
        int row = RandI(0, 5, RAND_OTHER);
        Tile& t = m_board[row][col];
        if (!IsSpecialEnemy(t)) {
            t.SetSpecialType(SPECIAL_TILE_CHAIN);
            AddFlare(1, m_gridPositions[row][col], kBlueFlare);
            break;
        }
    }
    StartBoardFlash();
}

void Game::CreateManaPotions() {
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            if (IsNormalPotion(m_board[row][col])) {
                AddFlare(1, m_gridPositions[row][col], kBlueFlare);
                m_board[row][col].MakeManaPotion();
            }
        }
    }
    StartBoardFlash();
}

void Game::CreateExplosivePotions() {
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            if (IsNormalPotion(m_board[row][col])) {
                // the original builds the colour in a local whose blue byte keeps the (false) acid flag
                AddFlare(1, m_gridPositions[row][col], GLcolour4{0xff, 0x96, 0, 0xff});
                m_board[row][col].MakeExplosivePotion();
            }
        }
    }
    StartBoardFlash();
}

void Game::FreezeSkulls() {
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            if (m_board[row][col].m_type == TILE_SKULL) {
                AddFlare(1, m_gridPositions[row][col], GLcolour4{100, 0xe6, 0xff, 0xff});
                m_board[row][col].SetFreezeVisibility(true);
            }
        }
    }
    StartBoardFlash();
}

void Game::CreateArrows() {
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            if (IsNormalSword(m_board[row][col])) {
                m_board[row][col].MakeArrow();
                AddFlare(1, m_gridPositions[row][col], kWhite);
            }
        }
    }
    StartBoardFlash();
}

void Game::CreateMagicSword() {
    int row = 0, col = 0;
    int index = RandI(0, GetNormalSwordCount() - 1, RAND_OTHER);
    if (Tile* t = PickTile(index, IsNormalSword, &row, &col)) {
        t->MakeMagicSword();
        AddFlare(1, m_gridPositions[row][col], kBlueFlare);
    }
    StartBoardFlash();
}

// Every `from` tile (skulls: normal ones only; swords: not arrows) becomes a new `to` tile.
void Game::ConvertTiles(TileType from, TileType to) {
    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 6; ++col) {
            Tile& t = m_board[row][col];
            if (from == TILE_SKULL && t.m_enemyType != ENEMY_NORMAL) continue;
            if (from == t.m_type && (from != TILE_SWORD || !t.m_arrow)) {
                t.SetType(to, m_turn, m_difficulty);
                AddFlare(1, m_gridPositions[row][col], kWhite);
            }
        }
    }
    StartBoardFlash();
    if (to == TILE_SKULL && from == TILE_COIN) m_coinsTurnedToSkulls = true;
}

void Game::DemoteSpecialEnemy() {
    int row = 0, col = 0;
    int index = RandI(0, GetSpecialEnemyCount() - 1, RAND_OTHER);
    if (Tile* t = PickTile(index, IsSpecialEnemy, &row, &col)) {
        t->m_enemyType = ENEMY_NORMAL;
        t->UpdateEnemyUV();
        t->m_countdown = 0;
        AddFlare(1, m_gridPositions[row][col], kGreenFlare);
    }
    StartBoardFlash();
}

// Big Game: a random normal enemy is promoted (without resetting the promotion timer).
void Game::PromoteNormalEnemy() {
    int row = 0, col = 0;
    int index = RandI(0, (GetTileCount(TILE_SKULL) - 1) - GetSpecialEnemyCount(), RAND_OTHER);
    if (Tile* t = PickTile(index, IsNormalEnemy, &row, &col)) {
        m_promotionTimerEnabled = false;
        t->Promote(m_turn, m_difficulty, true);
        m_promotionTimerEnabled = true;
        AddFlare(1, m_gridPositions[row][col], kFireFlare);
        if (t->m_enemyType == ENEMY_FLEEING) JumpDown(t);
    }
    StartBoardFlash();
}

// Shatter: normal enemies lose their armour and are reduced to attack 1. (No flash.)
void Game::ShatterDefence() {
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            Tile& t = m_board[row][col];
            if (!IsNormalEnemy(t)) continue;
            if (t.m_armour > 0) {
                AddParticles(&t, 1);
                t.m_armour = 0;
                t.UpdateText(false, true, false);
            }
            if (t.m_attack > 1) {
                AddParticles(&t, 3);
                t.m_attack = 1;
                t.UpdateText(true, false, false);
            }
        }
    }
}

void Game::Resurrect(Tile* tile) {
    QueueSound(SOUND_SHATTER);
    QueueSound(SOUND_SKULL);
    AddFlare(1, tile->m_position, kYellowFlare);
    AddSimpleLabel(0x26, &tile->m_position, 0, 0x2e, 1.0f);
    StartBoardFlash();
}

// Fleeing enemies jump to the bottom row of their column.
void Game::JumpDown(Tile* tile) {
    int col = tile->m_column;
    if (tile->m_row == 5) return;
    SwapTiles(col, 5, col, tile->m_row);
    const GLfloat2* p = &m_gridPositions[5][col];
    AddSimpleLabel(0x25, p, 0, 0x2d, 1.0f);
    AddFlare(1, *p, kWhite);
    StartBoardFlash();
}

void Game::FleeUp(Tile* tile) {
    int row = tile->m_row;
    int col = tile->m_column;
    const GLfloat2* p = &m_gridPositions[row][col];
    AddSimpleLabel(0x25, p, 0, 0x2d, 1.0f);
    if (row == 0) {
        MatchTile(tile, TILE_NONE);   // escapes off the top
        AddFlare(1, m_gridPositions[0][col], kWhite);
    } else {
        SwapTiles(col, row - 1, col, row);
        AddFlare(1, *p, kWhite);
        AddFlare(1, m_gridPositions[row - 1][col], kWhite);
        QueueSound(SOUND_JUMP);
        StartBoardFlash();
    }
}

// Teleport (skill): every tile is collected and the board refilled (no skulls).
void Game::Teleport() {
    for (int& c : m_chainResourceCounts) c = 0;
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            m_board[row][col].m_matched = true;
            AddFlare(1, m_gridPositions[row][col], GLcolour4{0, 100, 0xff, 0xff});
        }
    }
    if (GetSpecialEnemyCount() > 0) QueueAchievement(ACH_BRAVE_SIR_ROBIN, 1.0f);
    m_fillingNewBoard = true;
    MoveMatchedTiles();
    m_fillingNewBoard = false;
    ClearSpawnedFlags();
    StartBoardFlash();
}

// Unstable enemies: every unmatched tile is marked for a refill (no skulls; no swords when the
// class's mortal enemy is the Unstable one).
void Game::TeleportEnemy() {
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            Tile& t = m_board[row][col];
            if (!t.m_matched) {
                t.m_matched = true;
                AddFlare(1, m_gridPositions[row][col], GLcolour4{0, 100, 0xff, 0xff});
            }
        }
    }
    if (CharacterClass::GetMortalEnemyType(m_classes[m_currentClass].m_race) == ENEMY_UNSTABLE) {
        m_newBoardAvoidsSwords = true;
        m_fillingNewBoard = true;
        StartBoardFlash();
        QueueAchievement(ACH_UNFORESEEN_CONSEQUENCES, 100.0f);
    } else {
        m_newBoardAvoidsSwords = false;
        m_fillingNewBoard = true;
        StartBoardFlash();
    }
    QueueSound(SOUND_TELEPORT);
}

// Flaming enemies set the 8 surrounding unmatched tiles on fire (not other flaming enemies).
void Game::IgniteTiles(Tile* tile) {
    int r0 = tile->m_row;
    for (int dc = -1; dc <= 1; ++dc) {
        unsigned col = static_cast<unsigned>(tile->m_column + dc);
        for (int dr = -1; dr <= 1; ++dr) {
            int row = r0 + dr;
            if ((dr == 0 && dc == 0) || col >= 6 || row < 0 || row >= 6) continue;
            Tile& t = m_board[row][col];
            if (t.m_matched || (t.m_type == TILE_SKULL && t.m_enemyType == ENEMY_FLAMING)) continue;
            AddFlare(1, m_gridPositions[row][col], kFireFlare);
            t.SetFlameVisibility(true, tile->GetAttack(), false);
        }
    }
    StartBoardFlash();
}

// The Dragonkin's fire breath: a random normal enemy burns for the base damage.
void Game::FireBreath() {
    int row = 0, col = 0;
    int index = RandI(0, (GetTileCount(TILE_SKULL) - 1) - GetSpecialEnemyCount(), RAND_OTHER);
    if (Tile* t = PickTile(index, IsNormalEnemy, &row, &col)) {
        AddFlare(1, m_gridPositions[row][col], kFireFlare);
        t->SetFlameVisibility(true, m_baseDamage, true);
    }
    StartBoardFlash();
}

// Kamikaze enemies: explode (collected) and halve the player's health (min 1).
void Game::ExplodeEnemy(Tile* tile) {
    for (int& c : m_chainResourceCounts) c = 0;
    m_explosionSoundPlayed = false;
    MatchTile(tile, TILE_NONE);
    AddFlare(1, *m_interface.GetTarget(TILE_POTION), GLcolour4{0xff, 0, 0, 0xff});
    int health = m_health / 2;
    if (health < 1) health = 1;
    m_health = health;
    if (health != m_interface.m_health) {
        m_interface.m_health = health;
        m_interface.UpdateHealthBar(Interface::BAR_HEALTH, health);
    }
    StartBoardFlash();
}

// Golden enemies run away when their countdown ends.
void Game::RunAwayEnemy(Tile* tile) {
    AddFlare(1, tile->m_position, kYellowFlare);
    QueueSound(SOUND_JUMP);
    MatchTile(tile, TILE_NONE);
    MoveMatchedTiles();
    ClearSpawnedFlags();
    StartBoardFlash();
}

// Banish: a random special enemy is collected.
void Game::KillSpecialEnemy() {
    int row = 0, col = 0;
    int index = RandI(0, GetSpecialEnemyCount() - 1, RAND_OTHER);
    for (int& c : m_chainResourceCounts) c = 0;
    if (Tile* t = PickTile(index, IsSpecialEnemy, &row, &col)) {
        MatchTile(t, TILE_NONE);
        AddFlare(1, m_gridPositions[row][col], kWhite);
    }
    MoveMatchedTiles();
    if (!m_chainMadeThisTurn) ClearSpawnedFlags();
    StartBoardFlash();
}

// Assassins: their mortal-enemy version first kills the tile below (unless a special enemy);
// on the bottom row they stab for twice their attack.
void Game::StabAttack(Tile* tile) {
    if (CharacterClass::GetMortalEnemyType(m_classes[m_currentClass].m_race) == ENEMY_ASSASSIN) {
        int row = tile->m_row;
        if (row != 5) {
            Tile& below = m_board[row + 1][tile->m_column];
            if (IsSpecialEnemy(below)) return;
            MatchTile(&below, TILE_NONE);
        }
    }
    if (tile->m_row != 5) return;
    QueueSound(SOUND_ATTACK_HIT);
    QueueSound(SOUND_DAMAGE);
    AddSimpleLabel(0x13, &tile->m_position, 0, 0x1b, 1.0f);
    int health = m_health + tile->GetAttack() * -2;
    m_health = health < 0 ? 0 : health;
    if (m_interface.m_health != m_health) {
        m_interface.m_health = m_health;
        m_interface.UpdateHealthBar(Interface::BAR_HEALTH, m_health);
    }
    GLfloat2 p = Interface::GetPoisonPosition();
    AddSimpleLabel(2, &p, tile->GetAttack() << 1, 0xb, 0.5f);
}

void Game::Stab(Tile* tile) {
    QueueSound(SOUND_ATTACK_HIT);
    QueueSound(SOUND_DAMAGE);
    AddSimpleLabel(0x13, &tile->m_position, 0, 0x1b, 1.0f);
    int health = m_health - tile->GetAttack();
    m_health = health < 0 ? 0 : health;
    if (m_interface.m_health != m_health) {
        m_interface.m_health = m_health;
        m_interface.UpdateHealthBar(Interface::BAR_HEALTH, m_health);
    }
    GLfloat2 p = Interface::GetPoisonPosition();
    AddSimpleLabel(2, &p, tile->GetAttack(), 0xb, 0.5f);
    CheckEndGame();
}

// Greasy enemies: in every column the lowest skull with a non-skull below it (not the acting
// enemy) drops by one: the tile below it is collected.
void Game::LowerSkulls(Tile* tile) {
    for (int col = 0; col < 6; ++col) {
        for (int row = 4; row >= 0; --row) {
            if (m_board[row][col].m_type == TILE_SKULL && m_board[row + 1][col].m_type != TILE_SKULL &&
                !(col == tile->m_column && row == tile->m_row)) {
                MatchTile(&m_board[row + 1][col], TILE_NONE);
                AddFlare(1, m_gridPositions[row + 1][col], kEarthFlare);
                break;
            }
        }
    }
    AddSimpleLabel(0x17, &tile->m_position, 0, 0x1f, 1.0f);
    QueueSound(SOUND_POTION_START);
    StartBoardFlash();
}

void Game::MakeAcidPotion(Tile* tile) {
    int row = 0, col = 0;
    int index = RandI(0, GetNormalPotionCount() - 1, RAND_OTHER);
    if (GetNormalPotionCount() > 0) {
        AddSimpleLabel(0xe, &tile->m_position, 0, 0x17, 1.0f);
        QueueSound(SOUND_EXPLOSIVE);
    }
    if (Tile* t = PickTile(index, IsNormalPotion, &row, &col)) {
        t->MakeAcidPotion();
        AddFlare(1, m_gridPositions[row][col], kGreenFlare);
    }
    StartBoardFlash();
}

// Devourers: a random stat of a random item loses one upgrade.
void Game::DestroyUpgrade(Tile* tile) {
    int slot = RandI(0, static_cast<int>(m_itemSlots.size()) - 1, RAND_OTHER);
    Item* item = *m_itemSlots[slot];
    std::vector<int> stats;
    for (int i = 0; i < 20; ++i) {
        if (item->m_stats[i] > 0) stats.push_back(i);
    }
    if (!stats.empty()) {
        int i = RandI(0, static_cast<int>(stats.size()) - 1, RAND_OTHER);
        item->ReduceStat(static_cast<UpgradeType>(stats[i]));
        QueueAchievement(ACH_NOM_NOM, 100.0f);
    }
    AddSimpleLabel(0x20, &tile->m_position, 0, 0x28, 1.0f);
    QueueSound(SOUND_SHATTER);
    UpdateStats();
}

// Overseers: a random sword becomes a skull.
void Game::ConvertSwordToSkull(Tile* tile) {
    int row = 0, col = 0;
    int index = RandI(0, GetTileCount(TILE_SWORD) - 1, RAND_OTHER);
    if (GetTileCount(TILE_SWORD) > 0) {
        AddSimpleLabel(0x12, &tile->m_position, 0, 0x1a, 1.0f);
        QueueSound(SOUND_SKULL);
    }
    if (Tile* t = PickTile(index, [](const Tile& x) { return x.m_type == TILE_SWORD; }, &row, &col)) {
        t->SetType(TILE_SKULL, m_turn, m_difficulty);
        if (m_trapActive) {
            t->m_health = 1;
            t->UpdateText(false, false, true);
        }
        const GLfloat2* p = &m_gridPositions[row][col];
        AddFlare(1, *p, kFireFlare);
        AddSimpleLabel(0x12, p, 0, 0x1a, 1.0f);
    }
    StartBoardFlash();
}

// Hack: a random column (except special enemies) becomes swords; after Slash in the same turn the
// whole board is collected instead.
void Game::ConvertColumnToSwords() {
    if (m_slashUsedThisTurn) {
        m_hackUsedThisTurn = false;
        m_slashUsedThisTurn = false;
        CollectBoard();
        return;
    }
    m_hackUsedThisTurn = true;
    int col = RandI(0, 5, RAND_OTHER);
    for (int row = 0; row < 6; ++row) {
        if (IsSpecialEnemy(m_board[row][col])) continue;
        m_board[row][col].SetType(TILE_SWORD, m_turn, m_difficulty);
        AddFlare(1, m_gridPositions[row][col], kFireFlare);
    }
    StartBoardFlash();
}

void Game::CheckShockAwe() {
    if (m_collectRandomRowUsed && m_collectRandomBlockUsed && m_collectEdgeTilesUsed && m_collectBoardUsed) {
        QueueAchievement(ACH_SHOCK_AWE, 100.0f);
    }
}

// Clears the tiles' spawned-this-turn flags (0x8a) after a refill.
void Game::ClearSpawnedFlags() {
    for (auto& row : m_board) {
        for (Tile& t : row) t.m_spawnedThisTurn = false;
    }
}

// Collects matched tiles and refills; the 0x8a flags are cleared unless a chain was made.
void Game::FinishCollect() {
    MoveMatchedTiles();
    if (!m_chainMadeThisTurn) ClearSpawnedFlags();
}

// Awe (skill 41): shields and potions in the lower half are collected; after Shock in the same
// turn also a chain tile.
void Game::Awe() {
    m_aweUsed = true;
    for (int& c : m_chainResourceCounts) c = 0;
    m_explosionSoundPlayed = false;
    for (int col = 0; col < 6; ++col) {
        for (int row = 3; row < 6; ++row) {
            TileType t = m_board[row][col].m_type;
            if (t == TILE_SHIELD || t == TILE_POTION) MatchTile(&m_board[row][col], TILE_TYPE_COUNT);
            AddFlare(1, m_gridPositions[row][col], kEarthFlare);
        }
    }
    MoveMatchedTiles();
    if (m_shockUsed) CreateChain();
    if (!m_chainMadeThisTurn) ClearSpawnedFlags();
    StartBoardFlash();
}

// Shock (skill 40): shields and coins in the upper half; after Awe also a chain tile.
void Game::Shock() {
    m_shockUsed = true;
    for (int& c : m_chainResourceCounts) c = 0;
    m_explosionSoundPlayed = false;
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 3; ++row) {
            TileType t = m_board[row][col].m_type;
            if (t == TILE_SHIELD || t == TILE_COIN) MatchTile(&m_board[row][col], TILE_TYPE_COUNT);
            AddFlare(1, m_gridPositions[row][col], kEarthFlare);
        }
    }
    MoveMatchedTiles();
    if (m_aweUsed) CreateChain();
    if (!m_chainMadeThisTurn) ClearSpawnedFlags();
    StartBoardFlash();
}

// Heal/Repair/Treasure/Elixir: every tile of type `type` (potions: normal ones) is collected as
// `as`. (No flash.)
void Game::GrabTiles(TileType type, TileType as) {
    for (int& c : m_chainResourceCounts) c = 0;
    m_explosionSoundPlayed = false;
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            Tile& t = m_board[row][col];
            if (type != t.m_type) continue;
            if (type == TILE_POTION && (t.m_explosivePotion || t.m_manaPotion || t.m_acidPotion)) continue;
            MatchTile(&t, as);
        }
    }
    FinishCollect();
}

// Wish: everything but special enemies is collected.
void Game::CollectBoard() {
    m_collectBoardUsed = true;
    CheckShockAwe();
    m_collectingBoard = true;
    for (int& c : m_chainResourceCounts) c = 0;
    m_explosionSoundPlayed = false;
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            if (IsSpecialEnemy(m_board[row][col])) continue;
            MatchTile(&m_board[row][col], TILE_TYPE_COUNT);
            AddFlare(1, m_gridPositions[row][col], kFireFlare);
        }
    }
    FinishCollect();
    StartBoardFlash();
    m_collectingBoard = false;
}

// Charge: the border tiles (not special enemies).
void Game::CollectEdgeTiles() {
    m_collectEdgeTilesUsed = true;
    CheckShockAwe();
    for (int& c : m_chainResourceCounts) c = 0;
    m_explosionSoundPlayed = false;
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            bool edge = row == 0 || row == 5 || col == 0 || col == 5;
            if (!edge || IsSpecialEnemy(m_board[row][col])) continue;
            MatchTile(&m_board[row][col], TILE_TYPE_COUNT);
            AddFlare(1, m_gridPositions[row][col], kYellowFlare);
        }
    }
    FinishCollect();
    StartBoardFlash();
}

// Slash: a random row (normal skulls and other tiles; acid potions are destroyed, not collected).
// After Hack in the same turn the whole board is collected instead. All six tiles skulls:
// "Slasher".
void Game::CollectRandomRow() {
    m_collectRandomRowUsed = true;
    CheckShockAwe();
    if (m_hackUsedThisTurn) {
        m_hackUsedThisTurn = false;
        m_slashUsedThisTurn = false;
        CollectBoard();
        return;
    }
    m_slashUsedThisTurn = true;
    int row = RandI(0, 5, RAND_OTHER);
    for (int& c : m_chainResourceCounts) c = 0;
    m_explosionSoundPlayed = false;
    int skulls = 0;
    for (int col = 0; col < 6; ++col) {
        Tile& t = m_board[row][col];
        if (t.m_type == TILE_SKULL && t.m_enemyType != ENEMY_NORMAL) continue;
        if (t.m_type != TILE_SKULL && t.m_type == TILE_POTION && t.m_acidPotion) {
            MatchTile(&t, TILE_NONE);
        } else {
            MatchTile(&t, TILE_TYPE_COUNT);
        }
        if (t.m_type == TILE_SKULL) ++skulls;
        AddFlare(1, m_gridPositions[row][col], kYellowFlare);
    }
    FinishCollect();
    StartBoardFlash();
    if (skulls == 6) QueueAchievement(ACH_SLASHER, 100.0f);
}

// Fireball: a random 3x3 block (centre 1..4) like Slash.
void Game::CollectRandomBlock() {
    m_collectRandomBlockUsed = true;
    CheckShockAwe();
    int centreRow = RandI(1, 4, RAND_OTHER);
    int centreCol = RandI(1, 4, RAND_OTHER);
    for (int& c : m_chainResourceCounts) c = 0;
    m_explosionSoundPlayed = false;
    for (int col = centreCol - 1; col <= centreCol + 1; ++col) {
        for (int row = centreRow - 1; row <= centreRow + 1; ++row) {
            Tile& t = m_board[row][col];
            if (t.m_type == TILE_SKULL) {
                if (t.m_enemyType != ENEMY_NORMAL) continue;
                MatchTile(&t, TILE_TYPE_COUNT);
            } else if (t.m_type == TILE_POTION && t.m_acidPotion) {
                MatchTile(&t, TILE_NONE);
            } else {
                MatchTile(&t, TILE_TYPE_COUNT);
            }
            AddFlare(1, m_gridPositions[row][col], kFireFlare);
        }
    }
    FinishCollect();
    StartBoardFlash();
}

// Explosive Armour: every skull takes twice the armour as damage.
void Game::ExplodeArmour() {
    for (int& c : m_chainResourceCounts) c = 0;
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            if (m_board[row][col].m_type != TILE_SKULL) continue;
            const GLfloat2* p = &m_gridPositions[row][col];
            AddFlare(1, *p, kFireFlare);
            AddSimpleLabel(2, p, m_armour << 1, 0xb, 0.5f);
            AttackSkull(&m_board[row][col], m_armour << 1, false);
        }
    }
    FinishCollect();
    StartBoardFlash();
}

// Greed: every normal skull next to a coin (8 neighbours) is collected.
void Game::KillSkullsNextToCoins() {
    for (int& c : m_chainResourceCounts) c = 0;
    m_explosionSoundPlayed = false;
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            if (!IsNormalEnemy(m_board[row][col])) continue;
            bool coin = false;
            for (int c2 = col - 1; c2 <= col + 1 && !coin; ++c2) {
                for (int r2 = row - 1; r2 <= row + 1; ++r2) {
                    if ((r2 != row || c2 != col) && static_cast<unsigned>(c2) < 6 && static_cast<unsigned>(r2) < 6 &&
                        m_board[r2][c2].m_type == TILE_COIN) {
                        coin = true;
                        break;
                    }
                }
            }
            if (!coin) continue;
            MatchTile(&m_board[row][col], TILE_TYPE_COUNT);
            AddFlare(1, m_gridPositions[row][col], kFireFlare);
        }
    }
    FinishCollect();
    StartBoardFlash();
}

// ================================================================ abilities with tile sets / tokens

namespace {
// The index-th element (in address = board order) of a set, index = RandI(0, size - 1, RAND_OTHER).
std::set<Tile*>::iterator PickFromSet(std::set<Tile*>& tiles) {
    int index = RandI(0, static_cast<int>(tiles.size()) - 1, RAND_OTHER);
    auto it = tiles.begin();
    while (it != tiles.end() && index > 0) {
        ++it;
        --index;
    }
    return it;
}

float Distance(const GLfloat2& to, const GLfloat2& from) {
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    return std::sqrt(dy * dy + dx * dx);
}
}  // namespace

// The next token of the pool (grown by one when all are in use).
Token& Game::NextToken() {
    if (static_cast<size_t>(m_tokenCount) == m_tokens.size()) m_tokens.push_back(Token(&m_interface));
    return m_tokens[m_tokenCount];
}

// Trampling enemies break a random unbroken shield or sword (not arrows).
void Game::BreakRandomShieldSword(Tile* tile) {
    std::set<Tile*> tiles;
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            Tile& t = m_board[row][col];
            if (t.m_matched || t.m_broken) continue;
            if ((t.m_type == TILE_SWORD && !t.m_arrow) || t.m_type == TILE_SHIELD) tiles.insert(&t);
        }
    }
    if (tiles.empty()) return;
    auto it = PickFromSet(tiles);
    if (it != tiles.end()) {
        (*it)->MakeBroken();
        AddFlare(1, (*it)->m_position, kWhite);
    }
    AddSimpleLabel(0x16, &tile->m_position, 0, 0x1e, 1.0f);
    QueueSound(SOUND_ARMOUR_BREAK);
    StartBoardFlash();
}

// Chaotic enemies swap with a random other tile (not a special enemy), which is destroyed.
void Game::JumpEnemy(Tile* tile) {
    int col = tile->m_column;
    int row = tile->m_row;
    std::set<Tile*> tiles;
    for (int c = 0; c < 6; ++c) {
        for (int r = 0; r < 6; ++r) {
            Tile& t = m_board[r][c];
            if (!t.m_matched && !IsSpecialEnemy(t) && (c != col || r != row)) tiles.insert(&t);
        }
    }
    if (tiles.empty()) return;
    auto it = PickFromSet(tiles);
    int tcol = 0, trow = 0;
    if (it != tiles.end()) {
        tcol = (*it)->m_column;
        trow = (*it)->m_row;
    }
    SwapTiles(tcol, trow, col, row);
    MatchTile(&m_board[row][col], TILE_NONE);
    AddFlare(1, m_gridPositions[row][col], kWhite);
    const GLfloat2* p = &m_gridPositions[trow][tcol];
    AddFlare(1, *p, kWhite);
    AddSimpleLabel(0xf, p, 0, 0x18, 1.0f);
    QueueSound(SOUND_JUMP);
    StartBoardFlash();
}

// Alchemists turn two random tiles (not special enemies) into new random tiles.
void Game::ConvertRandomTiles(Tile* tile) {
    std::set<Tile*> tiles;
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            Tile& t = m_board[row][col];
            if (!t.m_matched && !IsSpecialEnemy(t)) tiles.insert(&t);
        }
    }
    for (int i = 0; i < 2 && !tiles.empty(); ++i) {
        auto it = PickFromSet(tiles);
        if (it == tiles.end()) continue;
        RandomiseTile(*it);
        AddFlare(1, (*it)->m_position, kYellowFlare);
        tiles.erase(it);
    }
    AddSimpleLabel(0x15, &tile->m_position, 0, 0x1d, 1.0f);
    QueueSound(SOUND_EXPLOSIVE);
    StartBoardFlash();
}

// Frozen enemies freeze two random unfrozen tiles (not special enemies).
void Game::FreezeTiles(Tile* tile) {
    std::set<Tile*> tiles;
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            Tile& t = m_board[row][col];
            if (!t.m_matched && !IsSpecialEnemy(t) && !t.m_frozen) tiles.insert(&t);
        }
    }
    for (int i = 0; i < 2 && !tiles.empty(); ++i) {
        auto it = PickFromSet(tiles);
        if (it == tiles.end()) continue;
        (*it)->SetFreezeVisibility(true);
        AddFlare(1, (*it)->m_position, GLcolour4{100, 0xe6, 0xff, 0xff});
        tiles.erase(it);
    }
    AddSimpleLabel(0x11, &tile->m_position, 0, 0x19, 1.0f);
    QueueSound(SOUND_FREEZE);
    StartBoardFlash();
}

// Alpha (skulls) / Magnetic (shields and swords): distant tiles of the kind swap with the other
// neighbours of the enemy until one of the two sets is used up.
void Game::AttractTiles(Tile* tile, bool skulls) {
    auto ofKind = [skulls](const Tile& t) {
        return skulls ? t.m_type == TILE_SKULL : (t.m_type == TILE_SWORD || t.m_type == TILE_SHIELD);
    };
    std::set<Tile*> distant;
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            Tile& t = m_board[row][col];
            int dc = col - tile->m_column;
            int dr = row - tile->m_row;
            if (!t.m_matched && ofKind(t) && (std::abs(dc) > 1 || std::abs(dr) > 1)) distant.insert(&t);
        }
    }
    if (distant.empty()) return;
    std::set<Tile*> near;
    for (int col = tile->m_column - 1; col <= tile->m_column + 1; ++col) {
        for (int row = tile->m_row - 1; row <= tile->m_row + 1; ++row) {
            if ((col == tile->m_column && row == tile->m_row) || static_cast<unsigned>(col) >= 6 || row < 0 || row >= 6) continue;
            Tile& t = m_board[row][col];
            if (t.m_matched || ofKind(t)) continue;
            near.insert(&t);
        }
    }
    if (near.empty()) return;
    while (!distant.empty() && !near.empty()) {
        auto a = PickFromSet(distant);
        auto b = PickFromSet(near);
        int c1 = (*a)->m_column, r1 = (*a)->m_row;
        int c2 = (*b)->m_column, r2 = (*b)->m_row;
        SwapTiles(c1, r1, c2, r2);
        GLcolour4 colour = skulls ? kYellowFlare : kWhite;
        AddFlare(1, m_gridPositions[r2][c2], colour);
        AddFlare(1, m_gridPositions[r1][c1], colour);
        distant.erase(a);
        near.erase(b);
    }
    if (skulls) {
        AddSimpleLabel(0x18, &tile->m_position, 0, 0x20, 1.0f);
        QueueSound(SOUND_SKULL);
    } else {
        AddSimpleLabel(0x19, &tile->m_position, 0, 0x21, 1.0f);
        QueueSound(SOUND_BOOST);
    }
    StartBoardFlash();
}

void Game::AttractSkulls(Tile* tile) {
    AttractTiles(tile, true);
}

void Game::AttractShieldsSwords(Tile* tile) {
    AttractTiles(tile, false);
}

// Brutes throw a random normal skull (column by column; not flagged 0x8a) at the player: it is
// collected and flies as a damage token carrying its health.
void Game::ThrowSkull(Tile* tile) {
    std::vector<Tile*> skulls;
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            Tile& t = m_board[row][col];
            if (!t.m_matched && IsNormalEnemy(t) && !t.m_spawnedThisTurn) skulls.push_back(&t);
        }
    }
    if (skulls.empty()) return;
    Tile* thrown = skulls[RandI(0, static_cast<int>(skulls.size()) - 1, RAND_OTHER)];
    MatchTile(thrown, TILE_NONE);
    AddFlare(1, thrown->m_position, kYellowFlare);
    Token& token = NextToken();
    token.InitialiseFromTile(thrown, TILE_SKULL, nullptr, 3);
    token.m_value = thrown->m_health;
    m_tokenCount += 1;
    AddSimpleLabel(0x1f, &tile->m_position, 0, 0x27, 1.0f);
    QueueSound(SOUND_ATTACK_MISS);
    StartBoardFlash();
}

// Healers: heal two damaged enemies (or, if none is damaged, bless two: +1 attack).
void Game::HealOrBless(Tile* tile) {
    std::set<Tile*> damaged;
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            Tile& t = m_board[row][col];
            if (col == tile->m_column && row == tile->m_row) continue;
            if (!t.m_matched && t.m_type == TILE_SKULL && t.m_health < t.m_maxHealth) damaged.insert(&t);
        }
    }
    if (!damaged.empty()) {
        QueueSound(SOUND_BLESS);
        HealOrBless(damaged, true, 2);
        StartBoardFlash();
        return;
    }
    std::set<Tile*> skulls;
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            Tile& t = m_board[row][col];
            if (col == tile->m_column && row == tile->m_row) continue;
            if (!t.m_matched && t.m_type == TILE_SKULL) skulls.insert(&t);
        }
    }
    if (skulls.empty()) return;
    QueueSound(SOUND_BLESS);
    HealOrBless(skulls, false, 2);
    StartBoardFlash();
}

void Game::HealOrBless(std::set<Tile*>& tiles, bool heal, int count) {
    if (count == 0 || tiles.empty()) return;
    auto it = PickFromSet(tiles);
    if (it != tiles.end()) {
        Tile* t = *it;
        if (!heal) {
            t->m_attack += 1;
            t->UpdateText(true, false, false);
            AddSimpleLabel(0xb, &t->m_position, 0, 0x14, 1.0f);
        } else {
            t->m_health = t->m_maxHealth;
            t->UpdateText(false, false, true);
            AddSimpleLabel(0xc, &t->m_position, 0, 0x15, 1.0f);
        }
        AddFlare(1, t->m_position, kBlueFlare);
        tiles.erase(it);
    }
    HealOrBless(tiles, heal, count - 1);
}

// Thieves: two coins fly to the thief; without coins on the board it takes 2 gold from the
// player (the Thief as the class mortal enemy also takes health).
void Game::StealGold(Tile* thief) {
    std::set<Tile*> coins;
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            Tile& t = m_board[row][col];
            if (t.m_type == TILE_COIN && !t.m_matched) coins.insert(&t);
        }
    }
    if (!coins.empty()) {
        AddSimpleLabel(0xd, &thief->m_position, 0, 0x16, 1.0f);
        StealGold(thief, coins, 2);
        StartBoardFlash();
        return;
    }
    if (m_gold > 0) {
        AddFlare(1, *m_interface.GetTarget(TILE_COIN), kYellowFlare);
        for (int i = 0; i < 2 && m_gold >= 1; ++i) {
            Token& coin = NextToken();
            coin.InitialiseFromTile(&m_board[0][0], TILE_COIN, nullptr, 0);
            coin.m_target = thief;
            if (thief) coin.m_distance = Distance(thief->m_position, coin.m_position);
            coin.m_position = *m_interface.GetTarget(TILE_COIN);
            if (coin.m_target) coin.m_distance = Distance(coin.m_target->m_position, coin.m_position);
            m_tokenCount += 1;
            m_gold -= 1;
            if (CharacterClass::GetMortalEnemyType(m_classes[m_currentClass].m_race) == ENEMY_THIEF) {
                Token& potion = NextToken();
                potion.InitialiseFromTile(&m_board[0][0], TILE_POTION, nullptr, 0);
                potion.m_target = thief;
                if (thief) potion.m_distance = Distance(thief->m_position, potion.m_position);
                potion.m_position = *m_interface.GetTarget(TILE_POTION);
                if (potion.m_target) potion.m_distance = Distance(potion.m_target->m_position, potion.m_position);
                m_tokenCount += 1;
                m_health = m_health - thief->GetAttack();
            }
        }
        AddSimpleLabel(0xd, &thief->m_position, 0, 0x16, 1.0f);
        if (m_gold != m_interface.m_gold) {
            m_interface.m_gold = m_gold;
            m_interface.UpdateGoldBar(Interface::BAR_GOLD, m_gold);
        }
        if (m_health != m_interface.m_health) {
            m_interface.m_health = m_health;
            m_interface.UpdateHealthBar(Interface::BAR_HEALTH, m_health);
        }
    }
    StartBoardFlash();
}

void Game::StealGold(Tile* thief, std::set<Tile*>& coins, int count) {
    if (count == 0 || coins.empty()) return;
    auto it = PickFromSet(coins);
    if (it != coins.end()) {
        Tile* c = *it;
        c->m_matched = true;
        Token& token = NextToken();
        token.InitialiseFromTile(c, TILE_COIN, nullptr, 0);
        token.m_target = thief;
        if (thief) token.m_distance = Distance(thief->m_position, token.m_position);
        m_tokenCount += 1;
        AddFlare(1, c->m_position, kYellowFlare);
        coins.erase(it);
    }
    StealGold(thief, coins, count - 1);
}
