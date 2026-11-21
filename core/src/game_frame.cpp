// Game: input and the per-frame logic (slice 3b). See game.h.
#include "game.h"

#include <cmath>
#include <cstdio>
#include <set>

#include "item.h"
#include "rand.h"
#include "util.h"

namespace {
// CalculateFPS's statics in the original (0x00089210 frames, 0x00089214 time, 0x00089218 text)
int g_fpsFrames = 0;
float g_fpsTime = 0.0f;
char g_fpsText[32];
}  // namespace

// (port: MapPointer: as the original, x and y * 1.5, except on the tall canvas: minus the screen's
// offset, or the horizontal layout's columns)
void Game::TouchStart(float x, float y, bool mouse) {
    m_touchIsMouse = mouse;
    m_touchState = TOUCHING;
    MapPointer(x, y, m_touchX, m_touchY);
    ResetIdle();   // port
}

void Game::TouchDrag(float x, float y) {
    MapPointer(x, y, m_touchX, m_touchY);
    ResetIdle();   // port
}

void Game::PointerMove(float x, float y) {
    ResetIdle();   // port
    m_hoverActive = true;
    MapPointer(x, y, m_hoverX, m_hoverY);
}

void Game::PointerLeave() { m_hoverActive = false; }

void Game::MouseWheel(float notches) {
    if (m_achievementsScreen.m_active) m_achievementsScreen.Scroll(notches);
}

// The title screen fading in (back from a game) or out (Continue), or the intro fading out (a new
// game): the board is under them.
bool Game::BoardUnderFade() const {
    if (m_scoreScreen.m_active || m_nameScreen.m_active) return false;
    const bool introClosing = m_introScreen.m_active && m_introScreen.m_closing;
    const bool menuFading = m_mainMenu.m_active && m_mainMenu.m_fadeTime > 0.0f;
    return (introClosing || !m_introScreen.m_active) && (menuFading || !m_mainMenu.m_active) &&
           (introClosing || menuFading);
}

bool Game::IsBoardScreen() const {
    return !m_scoreScreen.m_active && !m_nameScreen.m_active && !m_mainMenu.m_active && !m_introScreen.m_active;
}

void Game::TouchEnd() { m_touchState = FINISHED_TOUCHING; }
void Game::TouchInterrupted() { m_touchState = CANCELLED_TOUCHING; }

// Frame time from the clock (never negative); the clock is read twice (since, then now).
void Game::UpdateTime() {
    m_frameTime = GetTimeSince(static_cast<double>(m_lastTime));
    if (m_frameTime < 0.0f) m_frameTime = 0.0f;
    m_lastTime = static_cast<float>(GetTime());
}

// Frames per second over windows of more than 0.1 s, shown in the fps label.
void Game::CalculateFPS() {
    g_fpsFrames += 1;
    g_fpsTime = m_frameTime + g_fpsTime;
    if (g_fpsTime > 0.1f) {
        float fps = static_cast<float>(g_fpsFrames) / g_fpsTime;
        m_fps = fps;
        g_fpsTime = 0.0f;
        g_fpsFrames = 0;
        std::sprintf(g_fpsText, "fps: %.1f", static_cast<double>(fps));
        m_fpsLabel.SetText(g_fpsText, -1);
    }
}

void Game::QueueTileSound(Tile* tile) {
    switch (tile->m_type) {
        case TILE_POTION:
            QueueSound(tile->m_explosivePotion || tile->m_acidPotion ? SOUND_EXPLOSIVE : SOUND_POTION_START);
            break;
        case TILE_SKULL: QueueSound(SOUND_SKULL); break;
        case TILE_COIN: QueueSound(SOUND_COIN_START); break;
        case TILE_SHIELD: QueueSound(SOUND_SHIELD_START); break;
        case TILE_SWORD: QueueSound(SOUND_SWORD_START); break;
        case TILE_SPECIAL: QueueSound(SOUND_UP); break;
        default: break;
    }
}

std::set<Tile*>::iterator Game::GetSetIndex(std::set<Tile*>& set, int index) {
    auto it = set.begin();
    while (it != set.end()) {
        if (index == 0) return it;
        --index;
        ++it;
    }
    return set.begin();
}

// Tiles that cannot extend the chain (arrows not counted) are dimmed, the others undimmed.
void Game::UpdateDimming() {
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            Tile& t = m_board[row][col];
            if (t.CanMatchType(m_chain, true)) {
                t.Undim();
            } else {
                t.Dim();
            }
        }
    }
}

// The two halves of a broken shield fall apart over 0.5 s.
void Game::ProcessShieldShatter() {
    float t = (m_frameTime + m_frameTime) + m_shieldShatterTime;
    m_shieldShatterTime = t;
    if (t > 1.0f) {
        m_shieldShatterActive = false;
        return;
    }
    ProcessShieldChunk(&m_shieldShatterLeft, -1.0f, GLfloat2{0.4859375f, 1.29375f});
    ProcessShieldChunk(&m_shieldShatterRight, 1.0f, GLfloat2{0.5078125f, 1.29375f});
}

// Drifts sideways, falls on a parabola, spins by 90 degrees per unit of time, fades with t^3.
void Game::ProcessShieldChunk(Quad* chunk, float direction, GLfloat2 base) {
    float t = m_shieldShatterTime;
    chunk->m_position.x = base.x + (t * 0.08f) * direction;
    float s = Sqr(0.3f - t);
    chunk->m_position.y = base.y + (0.04f - s) * -0.2f;
    chunk->m_rotation = (direction * m_shieldShatterTime) * 90.0f;
    float fade = (1.0f - PowF(m_shieldShatterTime, 3.0f)) * 255.0f;
    chunk->SetAlpha(static_cast<uint8_t>(FloatToUintArm(fade)));
}

// While a chain is being built: a cross on every enemy the chain would kill (by its damage type;
// UPPERCUT adds the weapon damage) and on unselected Meat Shields the chain damage would kill; an
// "Uppercut" or "Blocked" label above enemies with those damage types (a second label pool, reset
// with each call). With no chain, all crosses are cleared.
void Game::UpdateCrossVisibility() {
    m_crossLabelCount = 0;
    if (m_chain.empty()) {
        for (auto& row : m_board) {
            for (Tile& t : row) t.m_crossVisible = false;
        }
        return;
    }
    auto addLabel = [this](int type, int colour, GLfloat2 position) {
        if (static_cast<unsigned>(m_crossLabels.size()) <= static_cast<unsigned>(m_crossLabelCount)) {
            m_crossLabels.push_back(SimpleLabel());
        }
        m_crossLabels[m_crossLabelCount].SetColour(colour);   // (colour before Initialise here)
        m_crossLabels[m_crossLabelCount].Initialise(type, position, 0, 1.0f);
        m_crossLabelCount += 1;
    };
    auto wouldDie = [](Tile* t, int damage) {
        int through = damage - t->m_armour;
        return t->m_health <= (through < 0 ? 0 : through);
    };
    for (auto it = m_chain.begin(); it != m_chain.end(); ++it) {
        int type = (*it)->GetDamageType(m_chain, it);
        Tile* t = *it;
        if (type == DAMAGE_NORMAL) {
            t->m_crossVisible = wouldDie(t, m_chainDamage);
        } else if (type == DAMAGE_UPPERCUT) {
            addLabel(0x23, 0x2b, GLfloat2{t->m_position.x, t->m_position.y - 0.0171875f});
            t->m_crossVisible = wouldDie(t, m_weaponDamage + m_chainDamage);
        } else {
            if (type == DAMAGE_BLOCKED) {
                addLabel(0x1e, 0x26, GLfloat2{t->m_position.x - 0.0109375f, t->m_position.y - 0.0171875f});
            }
            t->m_crossVisible = false;
        }
    }
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            Tile& t = m_board[row][col];
            if (!t.m_selected && t.m_type == TILE_SKULL && t.m_enemyType == ENEMY_MEAT_SHIELD) {
                t.m_crossVisible = wouldDie(&t, m_chainDamage);
            }
        }
    }
}

// The HUD previews of the chain being built: counts the chain's plain potions, killed skulls,
// coins and unbroken shields (not passed-over tiles; nothing but skulls with the Cocky flaw on
// chains of up to 3) on top of the counters' current values, keeps only the shields beyond what
// fills the armour, and shows the health, XP, upgrade point and gold the chain would give (twice
// in a Sprint challenge, whose score gain is set from them too).
void Game::ProcessResourceShadows() {
    bool cocky = false;
    if (m_chain.size() <= 3) cocky = m_classes[m_currentClass].HaveFlaw(FLAW_COCKY);
    for (Tile* t : m_chain) {
        if (m_chainSet.count(t)) continue;
        switch (t->m_type) {
            case TILE_POTION:
                if (!cocky && !t->m_explosivePotion && !t->m_manaPotion && !t->m_acidPotion) m_shadowPotions += 1;
                break;
            case TILE_SKULL:
                if (t->m_crossVisible) m_shadowKills += 1;
                break;
            case TILE_COIN:
                if (!cocky) m_shadowCoins += 1;
                break;
            case TILE_SHIELD:
                if (!cocky && !t->m_broken) m_shadowShields += 1;
                break;
            default:
                break;
        }
    }
    float shields = static_cast<float>(m_shadowShields);
    float missing = static_cast<float>(m_maxArmour - m_armour);
    int perShield = (m_bonusArmourAmount + m_baseArmourAmount) * (m_doubleArmour ? 2 : 1);
    float needed = std::ceil(missing / static_cast<float>(perShield));
    int extra = FloatToIntArm(shields - needed);
    m_shadowShields = extra < 0 ? 0 : extra;
    int sprint = (m_challenge && m_mode == MODE_SPRINT) ? 2 : 1;
    m_interface.SetHealthShadow(m_shadowPotions * (m_bonusHealAmount + m_baseHealAmount) * (m_doubleHealth ? 2 : 1));
    m_interface.SetXPshadow(FloatToIntArm(static_cast<float>(m_shadowKills * sprint) * (m_xpBonusRate + 2.0f)));
    m_interface.SetUPshadow(FloatToIntArm(static_cast<float>(m_shadowShields * sprint) * (m_upgradeBonusRate + 2.0f) *
                                          (m_doubleArmour ? 2.0f : 1.0f)));
    m_interface.SetGoldShadow(FloatToIntArm(static_cast<float>(m_shadowCoins * sprint) * (m_goldBonusRate + 2.0f) *
                                            (m_doubleGold ? 2.0f : 1.0f)));
    if (m_challenge && m_mode == MODE_SPRINT) {
        float score = static_cast<float>(m_shadowShields) * (m_upgradeBonusRate + 1.0f) * (m_doubleArmour ? 2.0f : 1.0f);
        score = score + static_cast<float>(m_shadowKills) * (m_xpBonusRate + 1.0f);
        score = score + static_cast<float>(m_shadowCoins) * (m_goldBonusRate + 1.0f) * (m_doubleGold ? 2.0f : 1.0f);
        m_scoreGain = FloatToIntArm(score);
    }
}

// Pretzel challenge: the chain's crossings (diagonal steps through an already crossed 2x2 block,
// as MatchTiles counts them) give the score gain: n, 2n, 3n for under 5, 10, 15 crossings, else 100.
void Game::ProcessPretzels() {
    std::set<int> blocks;
    int crossings = 0;
    int prevCol = 0;
    int prevRow = 0;
    for (auto it = m_chain.begin(); it != m_chain.end(); ++it) {
        int col = (*it)->m_column;
        int row = (*it)->m_row;
        if (it != m_chain.begin() && row != prevRow && col != prevCol) {
            int key = (prevCol < col ? prevCol : col) + (prevRow < row ? prevRow : row) * 6;
            if (blocks.count(key)) {
                crossings += 1;
            } else {
                blocks.insert(key);
            }
        }
        prevCol = col;
        prevRow = row;
    }
    if (crossings >= 15) {
        m_scoreBonus = 3;
        m_scoreGain = 100;
        return;
    }
    int factor = crossings < 5 ? 1 : crossings < 10 ? 2 : 3;
    m_scoreBonus = factor - 1;
    m_scoreGain = crossings * factor;
}

namespace {
// GLfloat2::Rescale (0x00024eb8): to length `length`, direction kept
void Rescale(GLfloat2& v, float length) {
    float l = std::sqrt(v.y * v.y + v.x * v.x);
    float x = v.x / l;
    float y = v.y / l;
    v.x = length * x;
    v.y = length * y;
}
}  // namespace

// The player's damage that one chain tile adds (port helper; inlined twice in the original):
// unbroken swords add the weapon damage (x5 magic, x2 double damage); the first skull of the chain
// (not a dead lich) adds the player's skull damage (halved while a Frozen enemy is the class's
// mortal enemy and on the board); burning tiles (not from Fire Breath) add their burn damage;
// a Spiky enemy marks the chain.
void Game::AddChainTileDamage(Tile* t) {
    if (t->m_type == TILE_SWORD) {
        if (!t->m_broken) {
            int damage = t->m_magicSword ? m_weaponDamage * 5 : m_weaponDamage;
            m_chainDamage = damage * (m_doubleDamage ? 2 : 1) + m_chainDamage;
        }
    } else if (t->m_type == TILE_SKULL && !m_chainHasSkull &&
               !(t->m_enemyType == ENEMY_LICH && t->m_countdown != 0)) {
        int damage = m_bonusDamage + m_baseDamage;
        if (CharacterClass::GetMortalEnemyType(m_classes[m_currentClass].m_race) == ENEMY_FROZEN &&
            GetEnemyCount(ENEMY_FROZEN) != 0) {
            damage = damage / 2;
        }
        m_chainDamage = m_chainDamage + damage;
        m_chainHasSkull = true;
    }
    if (t->m_burning && !t->m_burnIsFireBreath) m_chainBurnDamage = t->m_burnDamage + m_chainBurnDamage;
    if (t->m_type == TILE_SKULL && t->m_enemyType == ENEMY_SPIKY) m_chainHasSpiky = true;
}

// While dragging: if the touch has left the chain's last tile, the chain moves one step towards
// it (the step is the touch offset from the last tile, at most 1.2 tile sizes, rounded to a
// neighbour; the touch must be over that neighbour). Stepping back onto a chain tile cuts the
// chain after it (and recomputes the chain damage); a new tile is added if it can extend the
// chain (frozen non-skulls cannot) and is passed over (m_chainSet) when an arrow shoots past it.
// Then the life leech, Spiky damage, crosses and arrow are updated and the function repeats until
// the touch is on the last tile. Touching above or below the board drops the chain.
void Game::ProcessTileSelection() {
    if (m_chain.empty()) return;
    if (m_chain.back() == GetTileAtPosition(GLfloat2{m_touchX, m_touchY})) return;
    m_interface.HideTooltip();
    float top = m_boardOffsetY + 0.246875f;
    float y = m_touchY;
    if (y < top || y > top + 1.0f) {
        if (y < 0.146875f || y > 1.3468751f) {   // well above or below the board: drop the chain
            for (int col = 0; col < 6; ++col) {
                for (int row = 0; row < 6; ++row) m_board[row][col].Undim();
            }
            for (Tile* t : m_chain) {
                t->SetSize(GLfloat2{t->m_baseSize.x, t->m_baseSize.x});
                t->m_selected = false;
            }
            m_lifeLeechHeal = 0;
            m_chain.clear();
            m_chainSet.clear();
            m_chainDamage = 0;
            if ((m_keepChainBurnDamage & 0xff) == 0) m_chainBurnDamage = 0;
            if (!m_keepSpikyDamage) m_spikyDamage = 0;
            m_interface.SetHealthShadow(-1);
            m_interface.SetXPshadow(-1);
            m_interface.SetUPshadow(-1);
            m_interface.SetGoldShadow(-1);
            UpdateCrossVisibility();
        }
        return;
    }
    Tile* last = m_chain.back();
    GLfloat2 d{m_touchX - m_gridPositions[last->m_row][last->m_column].x,
               m_touchY - m_gridPositions[last->m_row][last->m_column].y};
    float limit = last->m_baseSize.x * 1.2f;
    if (limit < std::sqrt(d.y * d.y + d.x * d.x)) Rescale(d, limit);
    float half = m_halfTileSize;
    int dc = d.x < -half ? -1 : (d.x > half ? 1 : 0);   // (NaN -> 0, as the original's branches)
    int dr = -half > d.y ? -1 : (half < d.y ? 1 : 0);
    unsigned row = static_cast<unsigned>(dr + last->m_row);
    int col = dc + last->m_column;
    if (row >= 6 || col < 0 || col > 5) return;
    Tile* target = &m_board[row][col];
    if (!target->IsBeingSelected(GLfloat2{last->m_position.x + d.x, last->m_position.y + d.y})) return;

    bool found = false;   // the target is already in the chain: cut the chain after it
    for (auto it = m_chain.begin(); it != m_chain.end();) {
        if (found) {
            Tile* t = *it;
            t->SetSize(GLfloat2{t->m_baseSize.x, t->m_baseSize.x});
            t->m_selected = false;
            t->m_crossVisible = false;
            m_chainSet.erase(t);
            it = m_chain.erase(it);
        } else {
            found = *it == target;
            ++it;
        }
    }
    if (!found) {
        if (!target->CanMatchType(m_chain, false)) return;
        if (target->m_frozen && target->m_type != TILE_SKULL) return;
    }
    Tile* end = m_chain.back();
    bool passOver = false;   // an arrow shoots past non-sword, non-skull tiles
    if (end->m_type == TILE_SWORD && end->m_arrow && target->m_type != TILE_SWORD && target->m_type != TILE_SKULL) {
        passOver = true;
    } else {
        QueueTileSound(target);
    }
    if (found) {
        if (m_chainDamage > 0 || m_chainBurnDamage > 0) {   // recompute for the shortened chain
            m_chainDamage = 0;
            m_chainBurnDamage = 0;
            m_chainHasSkull = false;
            m_chainHasSpiky = false;
            for (Tile* t : m_chain) {
                if (m_chainSet.count(t)) continue;
                AddChainTileDamage(t);
            }
        }
    } else {
        float size = target->m_baseSize.x * 0.9f;
        target->SetSize(GLfloat2{size, size});
        target->m_selected = true;
        m_chain.push_back(target);
        if (passOver) {
            m_chainSet.insert(target);
        } else {
            AddChainTileDamage(target);
        }
    }
    float leech = static_cast<float>(m_lifeLeech) / 100.0f;
    m_lifeLeechHeal = FloatToIntArm(static_cast<float>(m_chainDamage) * leech);
    m_spikyDamage = m_chainHasSpiky ? m_chainDamage : 0;
    if (m_chainDamage > 0) UpdateCrossVisibility();
    if (m_chain.size() > 1) UpdateArrow();
    ProcessTileSelection();
}

// Rune skill: a chain of 4+ tiles whose ends touch encloses tiles. Rows 1..4, columns 0..4 are
// scanned left to right; crossing a chain tile toggles "inside" by where its neighbours in the
// (closed) chain lie (the usual ray test: one above and one below the row). An unselected tile
// inside, with a chain tile further right in its row, is enclosed: counted by type into
// m_shadowPotions.. (types 5+ in the sixth counter), then marked with a rune (preview, a pool of
// QuadStrips) or collected (collect: not special enemies; Runemaster when more tiles are collected
// than the chain holds outside m_chainSet). The marker count is reset only when the chain is too
// short; otherwise markers accumulate from m_runeMarkerCount (the callers reset it).
void Game::ProcessRune(bool collect) {
    if (!m_runeActive) return;
    if (m_chain.size() <= 3) {
        m_runeMarkerCount = 0;
        return;
    }
    Tile* first = m_chain.front();
    Tile* last = m_chain.back();
    int dcol = first->m_column - last->m_column;
    if ((dcol < 0 ? -dcol : dcol) > 1) return;
    int drow = first->m_row - last->m_row;
    if ((drow < 0 ? -drow : drow) > 1) return;
    unsigned collected = 0;
    for (int row = 1; row < 5; ++row) {
        bool inside = false;
        for (int col = 0; col < 5; ++col) {
            Tile& t = m_board[row][col];
            if (t.m_selected) {
                // the chain neighbours of this tile (the chain closes on itself)
                int prevRow = -1, prevCol = -1, nextRow = -1, nextCol = -1;
                bool found = false;
                for (auto it = m_chain.begin(); it != m_chain.end(); ++it) {
                    Tile* c = *it;
                    if (found) {
                        nextCol = c->m_column;
                        nextRow = c->m_row;
                        break;
                    }
                    if (c->m_column == col && c->m_row == row) {
                        found = true;
                    } else {
                        prevRow = c->m_row;
                        prevCol = c->m_column;
                    }
                }
                if (prevCol < 0) {
                    prevCol = last->m_column;
                    prevRow = last->m_row;
                } else if (nextCol < 0) {
                    nextCol = first->m_column;
                    nextRow = first->m_row;
                }
                if (prevRow == row && nextRow == row && col != prevCol && col != nextCol) continue;
                if (prevRow >= row && nextRow >= row) continue;
                if (prevRow < row && nextRow < row) continue;
                inside = !inside;
                continue;
            }
            if (!inside) continue;
            bool chainToTheRight = false;
            for (int c = col + 1; c < 6; ++c) {
                if (m_board[row][c].m_selected) {
                    chainToTheRight = true;
                    break;
                }
            }
            if (!chainToTheRight) break;   // (the rest of the row is outside)
            switch (t.m_type < 5 ? static_cast<int>(t.m_type) : 5) {   // the counters at 0x15cf8 + 4 * type
                case 0: m_shadowPotions += 1; break;
                case 1: m_shadowKills += 1; break;
                case 2: m_shadowCoins += 1; break;
                case 3: m_shadowShields += 1; break;
                case 4: m_shadowSwords += 1; break;
                default: m_shadowOthers += 1; break;
            }
            if (collect) {
                if (t.m_type == TILE_SKULL && t.m_enemyType != ENEMY_NORMAL) continue;
                collected += 1;
                MatchTile(&t, TILE_TYPE_COUNT);
                continue;
            }
            if (static_cast<unsigned>(m_runeMarkers.size()) <= static_cast<unsigned>(m_runeMarkerCount)) {
                m_runeMarkers.push_back(QuadStrip());
                QuadStrip& s = m_runeMarkers.back();
                s.m_textureSize = 0x400;
                s.AddQuad(GLfloat2{337.0f, 975.0f}, GLfloat2{387.0f, 1023.0f}, 2, -1.0f);
            }
            m_runeMarkers[m_runeMarkerCount].SetPositionCentre(m_gridPositions[row][col]);
            m_runeMarkerCount += 1;
        }
    }
    if (!collect || collected == 0) return;
    unsigned outside = static_cast<unsigned>(m_chain.size() - m_chainSet.size());
    if (collected > outside) QueueAchievement(ACH_RUNEMASTER, 100.0f);
}

// Once per frame. Waits while tiles fall, tokens fly or flares burn. After a chain (m_inputReady
// 0) the enemies attack (phase 1: ProcessAttack runs it; not in the first 5 turns with the
// Unexpected perk, or with no attacker) and EndOfTurn follows (phase 8); then input is allowed
// again. Touching: starts a chain on a tile (not a frozen non-skull; its damage, the enemy's
// tooltip), or drags it (ProcessTileSelection, rune markers, previews, dimming, pretzels).
// Releasing: a chain of 3+ tiles outside m_chainSet is matched (sounds, help, MatchTiles, the
// burn/Spiky/life leech label flights, Revenge, the Halfling's armour, Berserk healing); a single
// trophy tile unlocks or levels up a class; cancelling just drops the chain. Afterwards the chain
// is cleared.
void Game::ProcessInput() {
    if (!m_fallingTiles.empty()) return;
    if (m_tokenCount > 0) return;
    if (m_flareCount > 0) return;
    bool endOfTurn = false;
    if ((m_inputReady & 0xff) == 0) {
        if (m_attackPhase == 0) {
            if ((m_turn > 4 || !m_classes[m_currentClass].HavePerk(PERK_UNEXPECTED)) && CountAttackers() > 0) {
                m_attackPhase = 1;
                return;
            }
            m_attackPhase = 8;
            endOfTurn = true;
        }
    }
    if (!endOfTurn) {
        if (static_cast<unsigned>(m_attackPhase - 1) < 7) return;
        if (m_attackPhase == 8) endOfTurn = true;
    }
    if (endOfTurn) EndOfTurn();
    m_attackPhase = 0;
    m_inputReady = 1;
    if (m_touchState == NOT_TOUCHING) return;
    if (m_interface.m_pressedSkill >= 0) return;
    m_runeMarkerCount = 0;

    if (static_cast<unsigned>(m_touchState - FINISHED_TOUCHING) > 1) {   // still touching
        if (m_chain.empty()) {   // start a chain
            m_chainDamage = 0;
            m_chainHasSpiky = false;
            Tile* t = GetTileAtPosition(GLfloat2{m_touchX, m_touchY});
            if (t != nullptr && (!t->m_frozen || t->m_type == TILE_SKULL)) {
                m_chainHasSkull = false;
                if (t->m_type == TILE_SKULL) {
                    if (t->m_enemyType != ENEMY_LICH || t->m_countdown == 0) {
                        int damage = m_bonusDamage + m_baseDamage;
                        if (CharacterClass::GetMortalEnemyType(m_classes[m_currentClass].m_race) == ENEMY_FROZEN &&
                            GetEnemyCount(ENEMY_FROZEN) != 0) {
                            damage = damage / 2;
                        }
                        m_chainDamage = damage;
                        m_chainHasSkull = true;
                        if (t->m_enemyType != ENEMY_NORMAL) {
                            m_interface.ShowTooltip(t->m_enemyType, t->m_position);
                            if (t->m_enemyType == ENEMY_SPIKY) m_chainHasSpiky = true;
                        }
                    }
                } else if (t->m_type == TILE_SWORD && !t->m_broken) {
                    int damage = t->m_magicSword ? m_weaponDamage * 5 : m_weaponDamage;
                    m_chainDamage = damage * (m_doubleDamage ? 2 : 1);
                }
                if (t->m_burning && !t->m_burnIsFireBreath) m_chainBurnDamage = t->m_burnDamage;
                float size = t->m_baseSize.x * 0.9f;
                t->SetSize(GLfloat2{size, size});
                t->m_selected = true;
                m_chain.push_back(t);
                UpdateCrossVisibility();
                UpdateDimming();
                QueueTileSound(t);
            }
            m_shadowPotions = m_shadowKills = m_shadowCoins = m_shadowShields = m_shadowSwords = m_shadowOthers = 0;
            ProcessResourceShadows();
            float leech = static_cast<float>(m_lifeLeech) / 100.0f;
            m_lifeLeechHeal = FloatToIntArm(static_cast<float>(m_chainDamage) * leech);
            m_spikyDamage = m_chainHasSpiky ? m_chainDamage : 0;
            return;
        }
        if (m_chain.back() == GetTileAtPosition(GLfloat2{m_touchX, m_touchY})) {
            ProcessRune(false);
            return;
        }
        m_shadowPotions = m_shadowKills = m_shadowCoins = m_shadowShields = m_shadowSwords = m_shadowOthers = 0;
        ProcessTileSelection();
        ProcessRune(false);
        ProcessResourceShadows();
        UpdateDimming();
        if (m_challenge && m_mode == MODE_PRETZEL) ProcessPretzels();
        return;
    }

    for (int col = 0; col < 6; ++col) {   // released or cancelled
        for (int row = 0; row < 6; ++row) m_board[row][col].Undim();
    }
    if (m_touchState == CANCELLED_TOUCHING) {
        for (Tile* t : m_chain) {
            t->SetSize(GLfloat2{t->m_baseSize.x, t->m_baseSize.x});
            t->m_selected = false;
        }
        m_lifeLeechHeal = 0;
        m_interface.SetHealthShadow(-1);
        m_interface.SetXPshadow(-1);
        m_interface.SetUPshadow(-1);
        m_interface.SetGoldShadow(-1);
    } else if (!ReleaseChain()) {
        return;   // the game ended (trophy path)
    }
    m_chain.clear();
    m_chainSet.clear();
    m_chainDamage = 0;
    if ((m_keepChainBurnDamage & 0xff) == 0) m_chainBurnDamage = 0;
    if (!m_keepSpikyDamage) m_spikyDamage = 0;
    UpdateCrossVisibility();
}

// Part of ProcessInput (port helper): the touch was released. Returns false when the game ended
// (then the original returns before clearing the chain).
bool Game::ReleaseChain() {
    ProcessRune(true);
    bool skull = false;
    bool sword = false;
    for (Tile* t : m_chain) {
        t->SetSize(GLfloat2{t->m_baseSize.x, t->m_baseSize.x});
        t->m_selected = false;
        if (t->m_type == TILE_SKULL) {
            skull = true;
        } else if (t->m_type == TILE_SWORD) {
            sword = true;
        }
    }
    if (static_cast<unsigned>(m_chain.size() - m_chainSet.size()) > 2) {
        m_chainMadeThisTurn = true;
        if (skull) {
            QueueSound(SOUND_ATTACK_HIT);
        } else if (sword) {
            QueueSound(SOUND_ATTACK_MISS);
            if (m_helpMenu.m_shownHelp & (1 << 4)) {
                Tile* first = m_chain.front();
                ShowHelp(first->m_position.x, first->m_position.y, 0.0828125f, 0.0828125f, 4);
            }
        }
        m_chainKills = 0;
        m_chainKilledAttack = 0;
        m_tilesSpawned = 0;
        m_skullsSpawned = 0;
        MatchTiles();
        if (m_tilesSpawned > 9 && m_tilesSpawned == m_skullsSpawned) QueueAchievement(ACH_JUST_MESSING_WITH_YOU, 100.0f);
        if (m_chainBurnDamage > 0) {   // the burn damage label flies off
            m_burnLabelArc = Rand(0.4f, 0.8f, RAND_COSMETIC);
            m_burnLabelStart = m_simpleLabel14344.m_position;
            m_burnLabelT = 0.0f;
            m_keepChainBurnDamage = 1;
            m_burnLabelSpeed = 1.0f;
        }
        if (m_spikyDamage > 0) {   // the Spiky damage label flies off
            m_spikyLabelArc = Rand(0.4f, 0.8f, RAND_COSMETIC);
            m_spikyLabelStart = m_simpleLabel146b8.m_position;
            m_spikyLabelT = 0.0f;
            m_spikyLabelSpeed = 1.0f;
            m_keepSpikyDamage = true;
        }
        if (m_chainKills > 0) {
            AddRevengeHealth();
            if (m_chainKills > 14 && m_classes[m_currentClass].m_race == RACE_HALFLING) {
                m_items[2]->IncreaseStat(UPGRADE_ARMOUR);
                UpdateStats();
                GLfloat2 p{0.5f, 1.290625f};
                AddSimpleLabel(0x22, &p, 0, 0x2a, 1.0f);
                QueueAchievement(ACH_THE_BIGGER_THEY_ARE, 10.0f);
                if (m_coinsTurnedToSkulls) QueueAchievement(ACH_WHAT_DOESNT_KILL_YOU, 100.0f);
            }
        }
        if (m_berserkActive && m_chainKilledAttack > 0) {   // Berserk heals by the killed enemies' attack
            int maxHealth = m_bonusMaxHealth + m_baseMaxHealth;
            int health = m_chainKilledAttack + m_health;
            m_health = health < maxHealth ? health : maxHealth;
            QueueSound(SOUND_POTION);
            m_interface.SetHealth(m_health);
            AddSimpleLabel(0x1d, m_interface.GetTarget(TILE_POTION), m_chainKilledAttack, 0x25, 1.0f);
        }
        if (!m_chainHasSkull) {
            m_lifeLeechHeal = 0;
        } else {   // the life leech label flies off
            m_leechLabelArc = Rand(0.4f, 0.8f, RAND_COSMETIC);
            m_leechLabelStart = m_simpleLabelC388.m_position;
            m_leechLabelT = 0.0f;
            m_leechLabelSpeed = 1.0f;
        }
        return true;
    }
    m_lifeLeechHeal = 0;
    if (m_chain.size() != 1) return true;
    Tile* trophy = m_chain.front();
    if (trophy->m_type != TILE_SPECIAL || trophy->m_specialType != SPECIAL_TILE_TROPHY) return true;
    // a trophy: levels up the current class (half the time, if it can) or unlocks a random locked class
    CharacterClass& cls = m_classes[m_currentClass];
    bool levelCurrent = cls.m_level < 10 && cls.m_class != CLASS_ADVENTURER && RandI(0, 1, RAND_OTHER) != 0;
    if (!levelCurrent) {
        std::vector<int> locked;
        for (int c = 0; c < 10; ++c) {
            if (m_classes[c].m_level == 0) locked.push_back(c);
        }
        if (!locked.empty()) {
            int pick = locked[RandI(0, static_cast<int>(locked.size()) - 1, RAND_OTHER)];
            m_classes[pick].LevelUp();
            ShowUnlock(pick, 1);
        } else {
            levelCurrent = true;
        }
    }
    if (levelCurrent && m_classes[m_currentClass].m_level < 10) {
        m_classes[m_currentClass].LevelUp();
        ShowUnlock(m_classes[m_currentClass].m_class, m_classes[m_currentClass].m_level);
    }
    MatchTile(m_chain.front(), TILE_NONE);
    MoveMatchedTiles();
    if (CheckEndGame()) return false;
    SerialiseState();
    SerialiseClasses();
    return true;
}

// One frame. Resets the sound and achievement queues (the Java shell has read them), then runs
// the open screen (game over, help, unlock, main menu, name, intro, score)
// or the game: input (unless a HUD menu is open), a random help hint, shield shatter, tile
// effects, falling tiles, tokens delivering their resources, the particle and label pools, flares
// after a chain, the chain labels (damage, life leech, burn, Spiky) and their flights to the health
// bar, the enemies' attack; then the HUD. Released and cancelled touches are consumed.
void Game::Process() {
    UpdateTime();
    CalculateFPS();
    m_soundRead = 0;
    m_soundCount = 0;
    m_achievementCount = 0;
    m_achievementRead = 0;
    auto consumeRelease = [this]() {
        if (static_cast<unsigned>(m_touchState - FINISHED_TOUCHING) < 2) m_touchState = NOT_TOUCHING;
    };
    m_toast.Process(m_frameTime);      // port: over whatever runs
    ProcessIdle();                     // port: the idle animation (resets itself off the board)
    if (m_settingsScreen.m_active) {   // port: over the title screen or the paused game, which wait
        consumeRelease();
        m_settingsScreen.Process(m_frameTime);
        return;
    }
    if (m_achievementsScreen.m_active) {   // port: over the title screen, which waits
        consumeRelease();
        m_achievementsScreen.Process(m_frameTime);
        return;
    }
    if (m_gameOverScreen.m_active) {
        consumeRelease();
        m_gameOverScreen.Process(m_frameTime);
        if (m_gameOverScreen.m_state == 3) {
            if (!m_challenge) {
                ShowScores(m_difficulty, false);
            } else {
                ShowScores(m_mode, true);
            }
        }
        return;
    }
    if (m_helpMenu.m_active) {
        consumeRelease();
        m_helpMenu.Process(m_frameTime);
        return;
    }
    if (m_unlockMenu.m_active) {
        consumeRelease();
        m_unlockMenu.Process(m_frameTime);
        return;
    }
    if (m_mainMenu.m_active && !m_scoreScreen.m_active) {
        consumeRelease();
        if (m_nameScreen.m_active) {
            m_nameScreen.Process(m_frameTime);
            return;
        }
        if (m_gameOverScreen.m_active) return;
        m_debugPendingMenu = -1;   // port: a debug page's dialog is for the game that was left
        m_mainMenu.Process(m_frameTime);
        return;
    }
    if (m_nameScreen.m_active) {
        consumeRelease();
        m_nameScreen.Process(m_frameTime);
        return;
    }
    if (m_introScreen.m_active) {
        consumeRelease();
        m_introScreen.Process(m_frameTime);
        return;
    }
    if (m_scoreScreen.m_active) {
        consumeRelease();
        m_scoreScreen.Process(m_frameTime);
        return;
    }
    Interface& ui = m_interface;
    auto menuOpen = [&ui]() { return ui.m_foldoutMenu.m_active || ui.m_gameMenu.m_active || ui.m_statsMenu.m_active; };
    if (m_debugPendingMenu >= 0 && !menuOpen()) {   // port: the debug page's dialog, once its menus closed
        const int menu = m_debugPendingMenu;
        m_debugPendingMenu = -1;
        if (menu == 0) m_level += 1;   // as a level-up does (not banked: no score)
        ui.ShowMenu(menu);
    }
    if (!menuOpen()) {
        ProcessInput();
        if (menuOpen()) return;
        if (m_helpMenu.m_shownHelp & 1) {   // help page 0 at a random cell
            int r = RandI(0, 5, RAND_COSMETIC);
            int c = RandI(0, 5, RAND_COSMETIC);
            ShowHelp(m_gridPositions[r][c].x, m_gridPositions[r][c].y, 0.0f, 0.0f, 0);
        }
        if (m_shieldShatterActive) ProcessShieldShatter();
        for (int col = 0; col < 6; ++col) {
            for (int row = 0; row < 6; ++row) m_board[row][col].ProcessEffects(m_frameTime);
        }
        if (m_fallingTiles.empty()) {
            if (m_chain.empty() && (m_scoreGain > 0 || m_scoreBonus > 0)) {
                m_scoreGain = 0;
                m_scoreBonus = 0;
                m_chainPotionTokens = m_chainBonusPotionTokens = 0;
                m_chainSkullTokens = m_chainBonusSkullTokens = 0;
                m_chainCoinTokens = m_chainBonusCoinTokens = 0;
                m_chainShieldTokens = m_chainBonusShieldTokens = 0;
                m_chainSwordTokens = m_chainBonusSwordTokens = 0;
            }
        } else {
            for (auto it = m_fallingTiles.begin(); it != m_fallingTiles.end();) {
                (*it)->Process(m_frameTime);
                if (!(*it)->m_falling) {
                    it = m_fallingTiles.erase(it);
                } else {
                    ++it;
                }
            }
        }
        if (m_tokenCount > 0) {   // tokens deliver their resources when they arrive
            int lastAlive = -1;
            for (int i = 0; i < m_tokenCount; ++i) {
                Token& t = m_tokens[i];
                if (!t.m_alive) continue;
                t.Process(m_frameTime);
                if (t.m_alive) {
                    lastAlive = i;
                    continue;
                }
                if (t.m_target == nullptr) {
                    AddResource(t.m_type, t.m_kind, t.m_value);
                    if (menuOpen()) return;
                    if (m_health == 0) return;
                } else {
                    QueueSound(SOUND_COIN);   // a coin reaching a thief
                }
            }
            m_tokenCount = lastAlive + 1;
        }
        if (m_particleCount > 0) {
            int last = -1;
            for (int i = 0; i < m_particleCount; ++i) {
                Particles& p = m_particles[i];
                if (p.m_active) {
                    p.Process(m_frameTime);
                    if (p.m_active) last = i;
                }
            }
            m_particleCount = last + 1;
        }
        if (m_simpleLabelCount > 0) {
            int last = -1;
            for (int i = 0; i < m_simpleLabelCount; ++i) {
                SimpleLabel& l = m_simpleLabels[i];
                if (l.m_active) {
                    l.Process(m_frameTime);
                    if (l.m_active) last = i;
                }
            }
            m_simpleLabelCount = last + 1;
        }
        if (m_flareCount > 0 && m_attackPhase != 2 && m_attackPhase != 4 && m_attackPhase != 3) {
            m_boardFlashTime = m_boardFlashTime + (m_frameTime + m_frameTime);
            m_boardFlashLength = m_boardFlashLength + m_frameTime * -3.5f;
            if (m_boardFlashLength < 0.0f) {
                m_boardFlashLength = 0.0f;
                m_flareCount = 0;
            } else {
                for (int i = 0; i < m_flareCount; ++i) {
                    m_flares[i].SetAlpha(static_cast<uint8_t>(FloatToUintArm(m_boardFlashLength * 255.0f)));
                    m_flares[i].m_scale.x = m_boardFlashTime;
                    m_flares[i].m_scale.y = m_boardFlashTime;
                }
            }
        }
        if (!ProcessChainLabels()) return;
        ProcessAttack();
    }
    ui.Process(m_frameTime, (m_inputReady & 0xff) != 0);
    consumeRelease();
}

// One frame of a label's flight to the health bar (0.8625, 1.29375): accelerating on an arc,
// shrinking to 0.1. True when it arrived.
bool Game::ProcessLabelFlight(SimpleLabel& label, float& t, float& speed, const GLfloat2& start, float arc) {
    speed = speed + m_frameTime * 10.0f;
    t = t + speed * m_frameTime;
    if (t > 1.0f) t = 1.0f;
    GLfloat2 p = Interpolate(start, GLfloat2{0.8625f, 1.29375f}, t);
    p.y = p.y - (0.25f - Sqr(0.5f - t)) * arc;
    label.SetPosition(p);
    label.m_scale = Interpolate(0.1f, 1.0f, PowF(t, 5.0f));
    return 1.0f <= t;
}

// The chain's labels above the touch while dragging (damage, life leech, burn, Spiky damage); after
// a chain the life leech, burn and Spiky labels fly to the health bar and apply there. Returns false
// when the game ended (Process then stops).
bool Game::ProcessChainLabels() {
    float tx = m_touchX;
    float ty = m_touchY;
    // the chain damage
    SimpleLabel& damage = m_simpleLabelB904;
    if (damage.m_active && m_chainDamage == 0) {
        damage.Process(m_frameTime);
    } else if (m_chainDamage > 0) {
        if (!damage.m_active) {
            damage.Initialise(1, GLfloat2{tx, ty - 0.14f}, m_chainDamage, 1.0f);
        } else {
            damage.SetPosition(GLfloat2{tx, ty - 0.14f});
            damage.SetNumber(m_chainDamage);
            if (m_chainHasSkull) {
                ShowHelp(m_touchX, m_touchY - 0.14f, 0.05f, 0.0f, 8);
                m_interface.ShowHelpBaseDmg();
                int shown = m_helpMenu.m_shownHelp;
                m_interface.ShowHelpWeaponDmg();
                if ((shown & (1 << 10)) && (m_helpMenu.m_shownHelp & 0x400) == 0) {   // the help page took the touch
                    m_chainDamage = 0;
                    m_touchState = CANCELLED_TOUCHING;
                    ProcessInput();
                }
            }
        }
    }
    // the life leech heal
    SimpleLabel& leech = m_simpleLabelC388;
    if (leech.m_active && m_lifeLeechHeal == 0) {
        leech.Process(m_frameTime);
    } else if (m_lifeLeechHeal > 0) {
        if (m_chainDamage < 1) {
            if (ProcessLabelFlight(leech, m_leechLabelT, m_leechLabelSpeed, m_leechLabelStart, m_leechLabelArc)) {
                int health = m_lifeLeechHeal + m_health;
                m_health = health;
                int maxHealth = m_bonusMaxHealth + m_baseMaxHealth;
                if (maxHealth < health) m_health = maxHealth;
                m_interface.SetHealth(m_health);
                m_lifeLeechHeal = 0;
                QueueSound(SOUND_POTION);
            }
        } else {
            if (!leech.m_active) {
                leech.Initialise(8, GLfloat2{m_touchX, m_touchY - 0.1f}, m_lifeLeechHeal, 1.0f);
            } else {
                leech.SetPosition(GLfloat2{m_touchX, m_touchY - 0.1f});
                leech.SetNumber(m_lifeLeechHeal);
            }
            uint8_t alpha = m_chainHasSkull ? 0xff : 0x80;   // faded until the chain holds a skull
            leech.SetAlpha(alpha);
            leech.m_alpha = alpha;
        }
    }
    // the burn damage
    SimpleLabel& burn = m_simpleLabel14344;
    if (burn.m_active && m_chainBurnDamage == 0) {
        burn.Process(m_frameTime);
    } else if (m_chainBurnDamage > 0) {
        if ((m_keepChainBurnDamage & 0xff) == 0) {
            float off = m_lifeLeechHeal < 1 ? 0.1f : 0.060000002f;   // 0x3d75c290, not the float nearest 0.06
            if (!burn.m_active) {
                burn.Initialise(9, GLfloat2{m_touchX, m_touchY - off}, m_chainBurnDamage, 1.0f);
            } else {
                burn.SetPosition(GLfloat2{m_touchX, m_touchY - off});
                burn.SetNumber(m_chainBurnDamage);
            }
        } else if (ProcessLabelFlight(burn, m_burnLabelT, m_burnLabelSpeed, m_burnLabelStart, m_burnLabelArc)) {
            int health = m_health - m_chainBurnDamage;
            m_health = health;
            if (health < 0) {
                m_health = 0;
                QueueAchievement(ACH_TOASTED, 100.0f);
            }
            m_interface.SetHealth(m_health);
            m_chainBurnDamage = 0;
            m_keepChainBurnDamage = 0;
            QueueSound(SOUND_DAMAGE);
            if (CheckEndGame()) return false;
        }
    }
    // the Spiky enemies' damage
    SimpleLabel& spiky = m_simpleLabel146b8;
    if (spiky.m_active && m_spikyDamage == 0) {
        spiky.Process(m_frameTime);
    } else if (m_spikyDamage > 0) {
        if (!m_keepSpikyDamage) {
            float off = m_lifeLeechHeal < 1 ? 0.1f : 0.060000002f;   // 0x3d75c290, not the float nearest 0.06
            if (m_chainBurnDamage > 0) off = off - 0.04f;
            if (!spiky.m_active) {
                spiky.Initialise(10, GLfloat2{m_touchX, m_touchY - off}, m_spikyDamage, 1.0f);
            } else {
                spiky.SetPosition(GLfloat2{m_touchX, m_touchY - off});
                spiky.SetNumber(m_spikyDamage);
            }
        } else if (ProcessLabelFlight(spiky, m_spikyLabelT, m_spikyLabelSpeed, m_spikyLabelStart, m_spikyLabelArc)) {
            int health = m_health - m_spikyDamage;
            m_health = health;
            if (health < 0) {
                m_health = 0;
                QueueAchievement(ACH_IMPALED, 100.0f);
            }
            m_interface.SetHealth(m_health);
            m_spikyDamage = 0;
            m_keepSpikyDamage = false;
            QueueSound(SOUND_SLASH);
            if (CheckEndGame()) return false;
        }
    }
    return true;
}
