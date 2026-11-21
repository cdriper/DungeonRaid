// Game: collecting tiles, refilling the board, attacking enemies (slice 2b). See game.h.
#include "game.h"

#include <cmath>
#include <iterator>
#include <list>
#include <set>

#include "rand.h"
#include "skill.h"
#include "util.h"

namespace {
const GLcolour4 kRedFlare = {0xff, 0, 0, 0xff};
}  // namespace

// Collects a tile as resource `as` (TILE_TYPE_COUNT: its own type; TILE_NONE: destroyed without
// a resource). Kamikazes and explosive potions blow up their 3x3 neighbourhood; killed special
// enemies drop rewards and may become trophies; 4+ resources of a kind in the chain give a
// chance of a bonus token.
void Game::MatchTile(Tile* tile, TileType as) {
    if (tile->m_matched) return;
    if (as == TILE_TYPE_COUNT) as = tile->m_type;
    tile->m_matched = true;
    if (static_cast<int>(as) < 5) m_chainResourceCounts[as] += 1;
    bool enemy = false;
    if (tile->m_type == TILE_SKULL && tile->m_enemyType == ENEMY_KAMIKAZE) {
        enemy = true;
    } else {
        if (tile->m_broken) {
            QueueAchievement(ACH_PACK_RAT, 1.0f);
            return;
        }
        if (as == TILE_NONE || as == TILE_SWORD) return;
        if (tile->m_specialType != SPECIAL_TILE_NONE) return;
        enemy = tile->m_type == TILE_SKULL;
    }
    if (enemy) {
        AddParticles(tile, 0);
        if (tile->m_enemyType == ENEMY_KAMIKAZE) {
            Explode(tile, true);
        }
        if (as == TILE_NONE) return;
    }
    if (tile->m_type == TILE_POTION && tile->m_explosivePotion) {
        Explode(tile, false);
        return;
    }
    if (tile->m_type == TILE_SKULL && tile->m_enemyType != ENEMY_NORMAL) {
        KillSpecialEnemyTile(tile);
        return;
    }
    AddToken(tile, as, false, nullptr);
    if (tile->m_type == TILE_POTION && (tile->m_explosivePotion || tile->m_manaPotion || tile->m_acidPotion)) return;
    if (m_chainResourceCounts[as] < 4) return;
    int chance;
    switch (as) {
        case TILE_POTION: chance = m_bonusPotionTokenChance + m_basePotionTokenChance; break;
        case TILE_SKULL: chance = m_bonusSkullTokenChance + m_baseSkullTokenChance; break;
        case TILE_COIN: chance = m_bonusCoinTokenChance + m_baseCoinTokenChance; break;
        case TILE_SHIELD: chance = m_bonusShieldTokenChance + m_baseShieldTokenChance; break;
        default: chance = 0; break;
    }
    if (m_chainResourceCounts[as] > 9 && m_classes[m_currentClass].HavePerk(PERK_DARING)) chance = 100;
    if (chance < RandI(1, 100, RAND_OTHER)) return;
    ShowHelp(tile->m_position.x, tile->m_position.y, 0.04f, 0.0f, 1);
    AddToken(tile, as, true, nullptr);
    AddSimpleLabel(tile, as);
}

// The 3x3 explosion of a Kamikaze (without its own cell) or an explosive potion: normal enemies
// and other tiles are collected (special enemies survive; explosive potions explode in turn).
void Game::Explode(Tile* tile, bool kamikaze) {
    int col0 = tile->m_column;
    int row0 = tile->m_row;
    if (!m_explosionSoundPlayed) {
        QueueSound(SOUND_EXPLOSION);
        m_explosionSoundPlayed = true;
    }
    for (int dc = -1; dc <= 1; ++dc) {
        unsigned col = static_cast<unsigned>(col0 + dc);
        for (int dr = -1; dr <= 1; ++dr) {
            int row = row0 + dr;
            if (kamikaze && dc == 0 && dr == 0) continue;
            if (col >= 6 || row < 0 || row >= 6) continue;
            Tile& t = m_board[row][col];
            if (t.m_type == TILE_SKULL) {
                if (t.m_enemyType != ENEMY_NORMAL) continue;
                AddFlare(1, m_gridPositions[row][col], kRedFlare);
            } else if (t.m_type != TILE_POTION || !t.m_explosivePotion) {
                AddFlare(1, m_gridPositions[row][col], kRedFlare);
            }
            MatchTile(&t, TILE_TYPE_COUNT);
        }
    }
    AddFlare(1, m_gridPositions[row0][col0], kRedFlare);
    StartBoardFlash();
}

// A killed special enemy: reward tokens; on Normal and harder the trophy countdown (an enemy
// becomes a trophy every 20..30 kills, max 3 per game, not once every class is unlocked); the
// kill achievements; the Changeling shapeshift.
// Port: the Grind setting changes the trophy rate: original 20..30 kills and 3 per game, fast
// 10..15 and 5, faster 5..8 and no cap (a longer countdown from a slower setting is cut short).
namespace {
const int kTrophyKillsMin[3] = {20, 10, 5};
const int kTrophyKillsMax[3] = {30, 15, 8};
const int kTrophiesPerGame[3] = {3, 5, 36};
}  // namespace

// Port (the info panel): KillSpecialEnemyTile's countdown as the next kills will see it (cut to
// the Grind setting's maximum, at least 1 kill). 0: no trophy can come this game (the per-game cap,
// or every class unlocked while playing the Adventurer or a level 10 class); -1: Easy, no trophies.
// (Sprint challenge: each kill counts only half the time.)
int Game::TrophyKillsLeft() const {
    if (static_cast<int>(m_difficulty) <= 0) return -1;
    const int grind = ClampI(m_grind, 0, 2);
    if (m_trophiesSpawned >= kTrophiesPerGame[grind]) return 0;
    const CharacterClass& cls = m_classes[m_currentClass];
    if (cls.m_class == CLASS_ADVENTURER || cls.m_level > 9) {
        bool locked = false;
        for (int c = 1; c < 10; ++c) locked = locked || m_classes[c].m_level == 0;
        if (!locked) return 0;
    }
    int left = m_timeToTrophy;
    if (grind != 0 && left > kTrophyKillsMax[grind]) left = kTrophyKillsMax[grind];
    return left < 1 ? 1 : left;
}

void Game::KillSpecialEnemyTile(Tile* tile) {
    std::list<TileType> rewards;
    tile->GetRewards(rewards);
    for (TileType r : rewards) AddToken(&m_board[0][0], r, false, &tile->m_position);
    if (static_cast<int>(m_difficulty) > 0) {
        const int grind = ClampI(m_grind, 0, 2);
        if (grind != 0 && m_timeToTrophy > kTrophyKillsMax[grind]) m_timeToTrophy = kTrophyKillsMax[grind];
        if (!m_challenge || m_mode != MODE_SPRINT || RandI(0, 100, RAND_COSMETIC) < 50) m_timeToTrophy -= 1;
        m_timeToTrophyChanged = true;
        if (m_timeToTrophy < 1) {
            if (m_trophiesSpawned < kTrophiesPerGame[grind]) {
                CharacterClass& cls = m_classes[m_currentClass];
                bool spawn = true;
                if (cls.m_class == CLASS_ADVENTURER || cls.m_level > 9) {
                    spawn = false;   // only while some class is still locked
                    for (int c = 1; c < 10; ++c) {
                        if (m_classes[c].m_level == 0) {
                            spawn = true;
                            break;
                        }
                    }
                    if (!spawn) m_timeToTrophy = 0;
                }
                if (spawn) {
                    if (tile->m_promotedByBigGame) QueueAchievement(ACH_SAFARI, 100.0f);
                    int next = RandI(kTrophyKillsMin[grind], kTrophyKillsMax[grind], RAND_COSMETIC);
                    m_trophiesSpawned += 1;
                    m_timeToTrophy = next;
                    tile->SetSpecialType(SPECIAL_TILE_TROPHY);
                    tile->m_matched = false;
                }
            } else {
                m_timeToTrophy = 0;
            }
        }
    }
    QueueAchievement(ACH_BLOODLUST, 1.0f);
    if (tile->m_enemyType == ENEMY_CHAOTIC &&
        CharacterClass::GetMortalEnemyType(m_classes[m_currentClass].m_race) == ENEMY_CHAOTIC) {
        QueueAchievement(ACH_CANT_TOUCH_THIS, 100.0f);
    } else if (tile->m_enemyType == ENEMY_GOLDEN) {
        QueueAchievement(ACH_JACKPOT, 100.0f);
    }
    if (m_classes[m_currentClass].m_race == RACE_CHANGELING) {
        QueueAchievement(ACH_TEARS_IN_RAIN, 2.0f);
        Shapeshift();
    }
    ShowHelp(tile->m_position.x, tile->m_position.y, 0.0828125f, 0.0828125f, 0x16);
}

// A token for the tile's resource; counts the chain tokens and, in a sprint challenge, updates the
// chain score (shield tokens that only fill up the armour do not count).
void Game::AddToken(Tile* tile, TileType type, bool bonus, const GLfloat2* position) {
    Token& token = NextToken();
    token.InitialiseFromTile(tile, type, position, 0);
    m_tokenCount += 1;
    float x = tile->m_position.x, y = tile->m_position.y;
    switch (type) {
        case TILE_POTION:
            ShowHelp(x, y, 0.0828125f, 0.0828125f, 3);
            break;
        case TILE_SKULL:
            ShowHelp(x, y, 0.0828125f, 0.0828125f, 6);
            if (bonus) m_chainBonusSkullTokens += 1; else m_chainSkullTokens += 1;
            break;
        case TILE_COIN:
            ShowHelp(x, y, 0.0828125f, 0.0828125f, 2);
            if (bonus) m_chainBonusCoinTokens += 1; else m_chainCoinTokens += 1;
            break;
        case TILE_SHIELD:
            ShowHelp(x, y, 0.0828125f, 0.0828125f, 5);
            if (!tile->m_broken) {
                if (bonus) m_chainBonusShieldTokens += 1; else m_chainShieldTokens += 1;
            }
            break;
        default:
            break;
    }
    if (!(m_challenge && m_mode == MODE_SPRINT)) return;
    int armourMult = m_doubleArmour ? 2 : 1;
    float missing = static_cast<float>(m_maxArmour - m_armour);
    float perShield = static_cast<float>((m_bonusArmourAmount + m_baseArmourAmount) * armourMult);
    float needed = std::ceil(missing / perShield);
    int bonusShields = m_chainBonusShieldTokens;
    int shields = FloatToIntArm(static_cast<float>(m_chainShieldTokens) - needed);
    if (shields < 0) {
        bonusShields = shields + bonusShields;
        shields = 0;
    }
    float armourScale = m_doubleArmour ? 2.0f : 1.0f;
    float goldScale = m_doubleGold ? 2.0f : 1.0f;
    float gain = (static_cast<float>(shields) * (m_upgradeBonusRate + 1.0f)) * armourScale;
    gain = gain + static_cast<float>(m_chainSkullTokens) * (m_xpBonusRate + 1.0f);
    gain = gain + (static_cast<float>(m_chainCoinTokens) * (m_goldBonusRate + 1.0f)) * goldScale;
    m_scoreGain = FloatToIntArm(gain);
    float bonusGain = (static_cast<float>(bonusShields < 0 ? 0 : bonusShields) * (m_upgradeBonusRate + 1.0f)) * armourScale;
    bonusGain = bonusGain + static_cast<float>(m_chainBonusSkullTokens) * (m_xpBonusRate + 1.0f);
    bonusGain = bonusGain + (static_cast<float>(m_chainBonusCoinTokens) * (m_goldBonusRate + 1.0f)) * goldScale;
    m_scoreBonus = FloatToIntArm(bonusGain);
}

// Changelings: a random skill becomes ready.
void Game::Shapeshift() {
    if (m_skillCount == 0) return;
    int i = RandI(0, m_skillCount - 1, RAND_OTHER);
    m_skills[i]->m_charge = m_skills[i]->m_cooldown;
    m_interface.UpdateSkills();
}

// Gravity: every matched cell (bottom up, column by column) takes the nearest unmatched tile above
// it; without one a new random tile falls in from above the board. Fleeing enemies that land
// stab the player.
void Game::MoveMatchedTiles() {
    std::set<Tile*> fleeing;
    for (int col = 0; col < 6; ++col) {
        int newRow = 0;
        for (int row = 5; row >= 0; --row) {
            Tile& dest = m_board[row][col];
            if (!dest.m_matched) continue;
            float acceleration = static_cast<float>(row + 1) * 0.8f;
            int src = GetNearestUnmatchedTile(col, row);
            if (src < 0) {
                newRow -= 1;
                PositionTile(&dest, col, newRow, false);
                PositionTile(&dest, col, row, true);
                dest.m_fallAcceleration = acceleration;
                m_fallingTiles.push_back(&dest);
                dest.m_matched = false;
                dest.m_spawnedThisTurn = true;   // new this turn
                RandomiseTile(&dest);
                continue;
            }
            Tile& from = m_board[src][col];
            PositionTile(&from, col, row, true);
            from.m_column = col;
            from.m_row = row;
            from.m_fallAcceleration = acceleration;
            dest.m_column = col;
            dest.m_row = src;
            Tile copy(from);
            from = dest;
            dest = copy;
            m_fallingTiles.push_back(&dest);
            if (dest.m_type == TILE_SKULL && dest.m_enemyType == ENEMY_FLEEING) fleeing.insert(&dest);
        }
    }
    for (Tile* t : fleeing) Stab(t);
}

// Damage beyond the armour lowers health; the armour itself is pierced at the armour piercing
// chance per point. Kills are collected (a lich turns into a dead lich first). Chain attacks also
// steal coins (Steal), poison (items) and freeze (Orcs, 10%). Returns whether the enemy died.
bool Game::AttackSkull(Tile* tile, int damage, bool fromChain) {
    if (tile->m_enemyType == ENEMY_LICH && tile->m_type == TILE_SKULL && tile->m_countdown != 0) return false;
    if (fromChain && m_stealActive) AddToken(tile, TILE_COIN, false, nullptr);
    int armour = tile->m_armour;
    int through = damage - armour;
    int health = tile->m_health - (through < 0 ? 0 : through);
    if (health < 1) {
        if (tile->m_enemyType != ENEMY_LICH) {
            m_enemiesKilled += 1;
            m_chainKills += 1;
            m_chainKilledAttack = tile->GetAttack() + m_chainKilledAttack;
            MatchTile(tile, TILE_TYPE_COUNT);
            return true;
        }
        tile->m_countdown = 1;
        tile->UpdateEnemyUV();
        tile->m_lichJustDied = true;
        tile->m_attack = 0;
        tile->m_armour = 0;
        tile->m_health = 1;
        tile->m_poison[0] = tile->m_poison[1] = tile->m_poison[2] = 0;
        AddFlare(1, tile->m_position, GLcolour4{0xff, 0xff, 0, 0xff});
        StartBoardFlash();
        return false;
    }
    if (health != tile->m_health) {
        AddParticles(tile, 2);
        tile->m_health = health;
        tile->UpdateText(false, false, true);
        armour = tile->m_armour;
    }
    if (armour <= damage) damage = armour;
    if (damage > 0) {
        int pierced = 0;
        for (int i = 0; i < damage; ++i) {
            if (RandI(0, 100, RAND_OTHER) < m_armourPiercingChance) ++pierced;
        }
        if (pierced != 0) {
            AddParticles(tile, 1);
            tile->m_armour -= pierced;
            tile->UpdateText(false, true, false);
        }
    }
    if (fromChain) {
        if (m_poisonAttack > 0 && tile->m_enemyType != ENEMY_POISONOUS) {
            if (!m_poisonSoundPlayed) {
                QueueSound(SOUND_POISON);
                m_poisonSoundPlayed = true;
            }
            tile->AddPoison(m_poisonAttack);
            AddSimpleLabel(6, &tile->m_position, 0, 0xf, 1.0f);
        }
        if (m_classes[m_currentClass].m_race == RACE_ORC && RandI(1, 100, RAND_OTHER) < 11) {
            tile->SetFreezeVisibility(true);
            QueueAchievement(ACH_HAMMER_TIME, 2.0f);
        }
    }
    return false;
}

// --- the enemies' turn (helpers of ProcessAttack/EndOfTurn)

// Skulls that attack this turn: unfrozen, not refilled this turn, with attack > 0.
int Game::CountAttackers() {
    int count = 0;
    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 6; ++col) {
            Tile& t = m_board[row][col];
            if (t.m_type != TILE_SKULL || t.m_spawnedThisTurn || t.m_frozen) continue;
            if (t.GetAttack() > 0) ++count;
        }
    }
    return count;
}

// Counter: every skull (frozen ones too) is hit with its own attack, column by column; then the
// board is refilled and the refill flags are cleared.
void Game::DoCounterattack() {
    for (int& n : m_chainResourceCounts) n = 0;
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            Tile& t = m_board[row][col];
            if (t.m_type != TILE_SKULL || t.m_spawnedThisTurn) continue;
            AddSimpleLabel(2, &m_gridPositions[row][col], t.GetAttack(), 0xb, 0.5f);
            AttackSkull(&t, t.GetAttack(), false);
        }
    }
    MoveMatchedTiles();
    ClearSpawnedFlags();
}

// Vampires heal by their attack after attacking (up to their max health; without limit when the
// vampire is the class's mortal enemy), column by column.
void Game::ProcessVampires() {
    EnemyType mortal = CharacterClass::GetMortalEnemyType(m_classes[m_currentClass].m_race);
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            Tile& t = m_board[row][col];
            if (t.m_matched || t.m_type != TILE_SKULL || t.m_enemyType != ENEMY_VAMPIRE) continue;
            if (mortal == ENEMY_VAMPIRE) {
                t.AddHealth(t.GetAttack());
            } else {
                int health = t.m_health;
                if (health >= t.m_maxHealth) continue;
                health += t.GetAttack();
                t.m_health = health > t.m_maxHealth ? t.m_maxHealth : health;
                t.UpdateText(false, false, true);
            }
            AddSimpleLabel(0x1a, &t.m_position, t.GetAttack(), 0x22, 1.0f);
            QueueSound(SOUND_BOOST);
        }
    }
}

// Werewolves: normal enemies whose health has dropped below their armour are collected.
void Game::ProcessCripples() {
    if (m_classes[m_currentClass].m_race != RACE_WEREWOLF) return;
    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 6; ++col) {
            Tile& t = m_board[row][col];
            if (t.m_type != TILE_SKULL || t.m_enemyType != ENEMY_NORMAL || t.m_matched) continue;
            if (t.m_health >= t.m_armour) continue;
            MatchTile(&t, TILE_TYPE_COUNT);
            AddSimpleLabel(0x24, &t.m_position, 0, 0x2c, 1.0f);
            QueueAchievement(ACH_BITES_THE_DUST, 2.0f);
        }
    }
}

// Sums the incoming damage and poison of the attacking skulls (unfrozen, not refilled this turn)
// and puts eye flares on them (not on a dead lich, whose attack still counts).
void Game::InitialiseFlares() {
    m_flareCount = 0;
    m_incomingDamage = 0;
    m_incomingPoison = 0;
    const GLcolour4 white = {0xff, 0xff, 0xff, 0xff};
    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 6; ++col) {
            Tile& t = m_board[row][col];
            if (t.m_type != TILE_SKULL || t.m_spawnedThisTurn || t.m_frozen) continue;
            m_incomingDamage = t.GetAttack() + m_incomingDamage;
            if (t.m_enemyType == ENEMY_POISONOUS) m_incomingPoison = t.GetAttack() + m_incomingPoison;
            if (t.m_type == TILE_SKULL && t.m_enemyType == ENEMY_LICH && t.m_countdown != 0) continue;
            AddFlare(0, t.GetEyePosition(0), white);
            AddFlare(0, t.GetEyePosition(1), white);
        }
    }
}

// Scores a finished chain (m_chain, in chain order). Tiles in m_chainSet are passed over (not
// collected). Skulls take the chain damage (none when the damage type is NONE/BLOCKED, plus the
// weapon damage for an UPPERCUT); a dead lich (countdown) and all other tiles are collected (as
// nothing with the Cocky flaw on chains of up to 3). Meat Shields outside the chain take the chain
// damage too. Also: diagonal steps whose 2x2 blocks repeat are pretzel crossings (Pretzel
// challenge statistics and labels), and the chain achievements (Pretzels, Pretzel Legend, Pow
// Kisser, Sniper, Necklace, Around The World, Gold Digger). Then the board is refilled.
void Game::MatchTiles() {
    m_inputReady = 0;
    for (int& n : m_chainResourceCounts) n = 0;
    m_explosionSoundPlayed = false;
    m_poisonSoundPlayed = false;
    bool cocky = false;
    if (m_chain.size() <= 3) cocky = m_classes[m_currentClass].HaveFlaw(FLAW_COCKY);

    std::set<int> crossedBlocks;   // top-left cell (col + row * 6) of each diagonal step's 2x2 block
    int crossings = 0;
    bool allCoins = true;
    bool allEdge = true;
    bool prevPassedOver = false;   // the previous chain tile was in m_chainSet
    int prevCol = 0;
    int prevRow = 0;
    auto last = std::prev(m_chain.end());
    for (auto it = m_chain.begin(); it != m_chain.end(); ++it) {
        Tile* tile = *it;
        int col = tile->m_column;
        int row = tile->m_row;
        if (it != m_chain.begin() && col != prevCol && row != prevRow) {
            int key = (prevCol < col ? prevCol : col) + (prevRow < row ? prevRow : row) * 6;
            if (crossedBlocks.count(key)) {
                crossings += 1;
            } else {
                crossedBlocks.insert(key);
            }
        }
        if (allEdge && col != 0 && col != 5 && row != 0) allEdge = row == 5;
        prevCol = col;
        prevRow = row;
        if (m_chainSet.count(tile)) {
            prevPassedOver = true;
            allCoins = false;
            continue;
        }
        if (allCoins && tile->m_type != TILE_COIN) allCoins = false;
        bool attack = tile->m_type == TILE_SKULL && !(tile->m_enemyType == ENEMY_LICH && tile->m_countdown != 0);
        if (!attack) {
            if (tile->m_specialType == SPECIAL_TILE_CHAIN && m_chain.size() == 36) QueueAchievement(ACH_NECKLACE, 100.0f);
            MatchTile(tile, cocky ? TILE_NONE : TILE_TYPE_COUNT);
            prevPassedOver = false;
            continue;
        }
        int damage = m_chainDamage;
        bool uppercut = false;
        int type = tile->GetDamageType(m_chain, it);
        if (type == DAMAGE_NONE || type == DAMAGE_BLOCKED) {
            damage = 0;
        } else if (type == DAMAGE_UPPERCUT) {
            uppercut = true;
            damage += m_weaponDamage;
        }
        bool killed = AttackSkull(*it, damage, true);
        if (killed) {
            if (uppercut) QueueAchievement(ACH_POW_KISSER, 2.0f);
            // Sniper: an arrow, passed-over tiles, then a special enemy killed at the end of the
            // chain, more than one cell away from the arrow (the arrow is two chain tiles back)
            Tile* t = *it;
            // (with fewer than 3 chain tiles the original reads the list header as the arrow; never
            // happens in play, where chains have 3+ tiles)
            if (prevPassedOver && t->m_enemyType != ENEMY_NORMAL && it == last && m_chain.size() >= 3) {
                Tile* arrow = *std::prev(it, 2);
                int dr = arrow->m_row - t->m_row;
                int dc = arrow->m_column - t->m_column;
                if (arrow->m_type == TILE_SWORD && arrow->m_arrow && ((dc < 0 ? -dc : dc) > 1 || (dr < 0 ? -dr : dr) > 1)) {
                    QueueAchievement(ACH_SNIPER, 100.0f);
                }
            }
        }
        prevPassedOver = false;
    }

    for (int col = 0; col < 6; ++col) {   // Meat Shields not in the chain take the chain damage
        for (int row = 0; row < 6; ++row) {
            Tile& t = m_board[row][col];
            if (t.m_type != TILE_SKULL || t.m_enemyType != ENEMY_MEAT_SHIELD) continue;
            bool inChain = false;
            for (Tile* c : m_chain) {
                if (c == &t) {
                    inChain = true;
                    break;
                }
            }
            if (!inChain) AttackSkull(&t, m_chainDamage, false);
        }
    }
    if (crossings > 5) QueueAchievement(ACH_PRETZELS, 100.0f);
    if (m_challenge && m_mode == MODE_PRETZEL) {
        if (m_scoreGain > 0) {
            if (m_longestChain < crossings) m_longestChain = crossings;
            if (m_averageChainLength10 == 0) {
                m_averageChainLength10 = crossings * 10;
            } else {
                m_averageChainLength10 = (crossings * 10 + m_averageChainLength10 * m_chainCount) / (m_chainCount + 1);
            }
            m_chainCount += 1;
            static const GLfloat2 kCentre = {0.5f, 0.5f};   // a guarded static in the original (0x0008922c)
            if (crossings > 14) {
                QueueAchievement(ACH_PRETZEL_LEGEND, 100.0f);
                AddSimpleLabel(0x29, &kCentre, 0, 0x31, 1.0f);
            } else if (crossings > 9) {
                AddSimpleLabel(0x28, &kCentre, 0, 0x30, 1.0f);
            } else if (crossings > 4) {
                AddSimpleLabel(0x27, &kCentre, 0, 0x2f, 1.0f);
            }
        }
        m_pretzelScore = m_scoreGain + m_pretzelScore;
        m_scoreGain = 0;
        m_scoreBonus = 0;
    }
    if (allEdge && !m_chain.empty() && m_chain.size() == 20) QueueAchievement(ACH_AROUND_THE_WORLD, 100.0f);
    if (allCoins && !m_chain.empty() && m_chain.size() == 36) QueueAchievement(ACH_GOLD_DIGGER, 100.0f);
    ClearSpawnedFlags();
    MoveMatchedTiles();
}
