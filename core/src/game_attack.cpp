// Game: the enemies' attack animation (ProcessAttack) and the eye flares. See game.h.
//
// ProcessAttack runs once per frame while m_attackPhase is 1..7 (ProcessInput starts it after a
// chain and waits for phase 8): the screen darkens (1), the attackers' eyes flare (2), then either
// the Counter skill's banner and counterattack (3) or the incoming damage banner (4), whose label
// flies to the armour bar (5, armour absorbs and may break; Attack Reduction and Counter Damage
// hit back) and to the health bar (6, health and poison are applied), then everything fades (7).
// All float steps follow the original's operation order (vmla unfused).
#include "game.h"

#include "rand.h"
#include "skill.h"
#include "util.h"

namespace {
constexpr float kPx = 1.0f / 1024.0f;   // texture pixel

// vcvt.u32.f32 + uxtb, as the original converts the 0..255 alpha values
uint8_t AlphaU8(float value) { return static_cast<uint8_t>(FloatToUintArm(value)); }
}  // namespace

// A flare (type 1: star, else: eye glint) at a position; the pool reuses its Quads.
void Game::AddFlare(int type, GLfloat2 position, const GLcolour4& colour) {
    if (m_flareCount == static_cast<int>(m_flares.size())) m_flares.push_back(Quad());
    Quad& f = m_flares[m_flareCount];
    if (type == 1) {
        f.SetUV(GLfloat2{209 * kPx, 321 * kPx}, GLfloat2{268 * kPx, 381 * kPx});
    } else {
        f.SetUV(GLfloat2{478 * kPx, 437 * kPx}, GLfloat2{501 * kPx, 460 * kPx});
    }
    f.m_scale = GLfloat2{0.08f, 0.08f};
    f.m_position = position;
    f.m_rotation = Rand(0.0f, 360.0f, RAND_COSMETIC);
    f.SetColour(colour);
    m_flareCount += 1;
}

void Game::ProcessAttack() {
    int phase = m_attackPhase;
    if (phase == 1) {   // the shade fades in up to alpha 200, then the eyes flare
        int a = FloatToIntArm(static_cast<float>(m_attackShade.m_vertices[0].colour.a) + m_frameTime * 800.0f);
        uint8_t alpha;
        if (a <= 200) {
            alpha = static_cast<uint8_t>(a);
        } else {
            QueueSound(SOUND_SKULL);
            m_attackPhase = 2;
            m_boardFlashTime = 0.0f;
            m_boardFlashLength = 1.0f;
            InitialiseFlares();
            alpha = 200;
        }
        m_attackShade.SetAlpha(alpha);
        m_interface.SetHealthShadow(-1);
        m_interface.SetXPshadow(-1);
        m_interface.SetUPshadow(-1);
        m_interface.SetGoldShadow(-1);
        phase = m_attackPhase;
    }

    if (phase == 2 || phase == 4 || phase == 3) {
        // the flares grow (time) and fade (length, from 1 at -4.5/s)
        m_boardFlashTime = m_boardFlashTime + m_frameTime;
        m_boardFlashLength = m_boardFlashLength + m_frameTime * -4.5f;
        bool flaresDone = false;
        if (m_boardFlashLength < 0.5f) {
            if (m_attackPhase == 2) {
                if (!m_counterActive) {
                    m_attackPhase = 4;
                    m_attackFade = 0.0f;
                    m_incomingDamageLabel.SetNumber(m_incomingDamage);
                    m_incomingDamageLabel.SetPosition(GLfloat2{0.5f, m_incomingLabelY});
                    m_incomingDamageLabel.m_scale = 1.0f;
                    m_incomingPoisonLabel.SetNumber(m_incomingPoison);
                    m_incomingPoisonLabel.SetPosition(GLfloat2{0.5f, m_incomingLabelY + 0.05f});
                    m_incomingPoisonLabel.m_scale = 1.0f;
                } else {
                    QueueSound(SOUND_ATTACK_HIT);
                    m_attackPhase = 3;
                    m_attackFade = 0.0f;
                }
            }
            if (m_boardFlashLength < 0.0f) {   // (NaN keeps the flares, as the original's bpl)
                m_flareCount = 0;
                m_boardFlashLength = 0.0f;
                flaresDone = true;
            }
        }
        if (!flaresDone) {
            for (int i = 0; i < m_flareCount; ++i) {
                m_flares[i].SetAlpha(AlphaU8(m_boardFlashLength * 255.0f));
                m_flares[i].m_scale.x = m_boardFlashTime;
                m_flares[i].m_scale.y = m_boardFlashTime;
            }
        }

        if (m_attackPhase == 3) {   // counterattack banner fades in; the counterattack once the flares are gone
            float f = m_attackFade + m_frameTime * 3.0f;
            m_attackFade = f;
            uint8_t alpha;
            if (f > 1.0f) {
                m_attackFade = 1.0f;
                if (flaresDone) {
                    m_attackPhase = 7;
                    DoCounterattack();
                    alpha = AlphaU8(m_attackFade * 255.0f);
                } else {
                    alpha = 0xff;
                }
            } else {
                alpha = AlphaU8(f * 255.0f);
            }
            m_counterBanner.SetAlpha(alpha);
        }

        if (m_attackPhase == 4) {   // damage banner fades in; then the label flies to armour or health
            float f = m_attackFade + m_frameTime * 8.0f;
            m_attackFade = f;
            if (f > 1.0f) {
                m_attackFade = 1.0f;
                if (flaresDone) {
                    if (m_armour < 1) {
                        m_attackPhase = 6;
                        m_damageLabelTarget.y = 1.29375f;
                        m_damageLabelTarget.x = 0.8625f;
                        m_damageLabelEndScale = 0.2f;
                        m_damageLabelArc = Rand(0.4f, 0.8f, RAND_COSMETIC);
                    } else {
                        m_attackPhase = 5;
                        m_damageLabelTarget.y = 1.290625f;
                        m_damageLabelTarget.x = 0.5f;
                        m_damageLabelEndScale = 0.8f;
                        m_damageLabelArc = Rand(0.4f, 1.0f, RAND_COSMETIC);
                    }
                    m_damageLabelStart = m_incomingDamageLabel.m_position;
                    m_damageLabelSpeed = 1.0f;
                    m_damageLabelT = 0.0f;
                    ShowHelp(m_damageLabelStart.x, m_damageLabelStart.y, 0.0f, 0.0f, 0x10);
                }
            }
            m_damageLabelStartScale = m_incomingDamageLabel.m_scale;
            m_attackBanner.SetAlpha(AlphaU8(m_attackFade * 255.0f));
            m_incomingDamageLabel.SetAlpha(AlphaU8(m_attackFade * 255.0f));
            m_incomingPoisonLabel.SetAlpha(AlphaU8(m_attackFade * 255.0f));
        }
        phase = m_attackPhase;
    }

    if (phase == 5 || phase == 6) {   // the damage label's flight: accelerating, on an arc, shrinking
        float speed = m_damageLabelSpeed + m_frameTime * 20.0f;
        m_damageLabelSpeed = speed;
        float t = m_damageLabelT + speed * m_frameTime;
        m_damageLabelT = t;
        bool arrived = false;
        if (t > 1.0f) {
            t = 1.0f;
            m_damageLabelT = 1.0f;
            arrived = true;
        }
        GLfloat2 p = Interpolate(m_damageLabelStart, m_damageLabelTarget, t);
        float arc = (0.25f - Sqr(0.5f - m_damageLabelT)) * m_damageLabelArc;
        if (m_attackPhase == 5) {
            p.x = p.x - arc;
        } else {
            p.y = p.y - arc;
        }
        m_incomingDamageLabel.SetPosition(p);
        m_incomingDamageLabel.m_scale =
            Interpolate(m_damageLabelEndScale, m_damageLabelStartScale, PowF(m_damageLabelT, 5.0f));
        m_incomingPoisonLabel.SetPosition(GLfloat2{p.x, p.y + 0.05f});
        m_incomingPoisonLabel.m_scale = m_incomingDamageLabel.m_scale;
        if (arrived) {
            if (m_attackPhase == 5) {
                AttackArmour();
            } else {
                AttackHealth();
            }
        }
    }

    if (m_attackPhase == 7) {   // everything fades out at 900/s
        int a = FloatToIntArm(static_cast<float>(m_attackShade.m_vertices[0].colour.a) + m_frameTime * -900.0f);
        uint8_t alpha;
        if (a < 0) {
            m_attackPhase = 8;
            alpha = 0;
        } else {
            alpha = static_cast<uint8_t>(a);
        }
        m_attackShade.SetAlpha(alpha);
        if (m_counterActive) {
            m_counterBanner.SetAlpha(alpha);
        } else {
            m_attackBanner.SetAlpha(alpha);
        }
    }
}

// Part of ProcessAttack (port helper): the label reached the armour bar. The armour absorbs what
// it can (each absorbed point breaks a point of armour with the armour damage chance, always while
// a Defiler is on the board); the rest flies on to the health bar (phase 6) or, if nothing is
// left, the attack ends (phase 7). Then Attack Reduction weakens the skulls and Counter Damage
// hits every skull back.
void Game::AttackArmour() {
    m_attackPhase = 6;
    m_damageLabelTarget.y = 1.29375f;
    m_damageLabelTarget.x = 0.8625f;
    m_damageLabelStart = m_incomingDamageLabel.m_position;
    m_damageLabelSpeed = 1.0f;
    m_damageLabelT = 0.0f;
    m_damageLabelStartScale = m_incomingDamageLabel.m_scale;
    m_damageLabelEndScale = 0.2f;
    m_damageLabelArc = Rand(0.4f, 0.8f, RAND_COSMETIC);
    int shown = m_incomingDamage < m_armour ? m_incomingDamage : m_armour;
    AddSimpleLabel(2, &m_incomingDamageLabel.m_position, -shown, 0xc, 0.5f);
    ShowHelp(m_incomingDamageLabel.m_position.x, m_incomingDamageLabel.m_position.y, 0.0f, 0.1f, 0x11);
    int incoming = m_incomingDamage;
    int absorbed = incoming < m_armour ? incoming : m_armour;
    incoming -= m_armour;
    m_incomingDamage = incoming;
    if (incoming < 1) {
        m_attackPhase = 7;
        if (m_incomingPoison > 0) {
            AddSimpleLabel(7, &m_incomingPoisonLabel.m_position, m_incomingPoison, 0xf, 0.5f);
            m_incomingPoison = 0;
        }
    } else {
        m_incomingDamageLabel.SetNumber(incoming);
    }
    int defilers = GetEnemyCount(ENEMY_DEFILER);
    int broken = 0;
    for (int i = 0; i < absorbed; ++i) {
        if (defilers > 0 || RandI(0, 100, RAND_OTHER) < m_armourDamageChance) ++broken;
    }
    if (broken == 0) {
        QueueSound(SOUND_ARMOUR);
    } else {
        m_armour -= broken;
        if (m_armour != m_interface.m_armour) {
            m_interface.m_armour = m_armour;
            m_interface.UpdateShieldValue();
        }
        InialiseShieldShatter();
        QueueSound(SOUND_ARMOUR_BREAK);
    }

    if (m_attackReduction > 0) {   // skulls with attack 2+ lose attack (down to 1), column by column
        bool first = true;
        for (int col = 0; col < 6; ++col) {
            for (int row = 0; row < 6; ++row) {
                Tile& t = m_board[row][col];
                if (t.m_type != TILE_SKULL || t.m_spawnedThisTurn || t.m_attack < 2) continue;
                if (first) QueueSound(SOUND_BLUNT);
                int attack = t.m_attack - m_attackReduction;
                t.m_attack = attack < 1 ? 1 : attack;
                t.UpdateText(true, false, false);
                GLfloat2 at = t.GetAttackLabelPosition();
                AddSimpleLabel(4, &at, m_attackReduction, 0xe, 1.0f);
                AddParticles(&t, 3);
                first = false;
            }
        }
    }

    if (m_counterDamage > 0) {   // every skull (frozen too) takes the counter damage, column by column
        bool first = true;
        for (int& n : m_chainResourceCounts) n = 0;
        for (int col = 0; col < 6; ++col) {
            for (int row = 0; row < 6; ++row) {
                Tile& t = m_board[row][col];
                if (t.m_type != TILE_SKULL || t.m_spawnedThisTurn) continue;
                if (first) QueueSound(SOUND_SWORD_START);
                EnemyType type = t.m_enemyType;
                AddSimpleLabel(5, &t.m_position, m_counterDamage, 0x10, 1.0f);
                if (AttackSkull(&t, m_counterDamage, false) && type == ENEMY_SPIKY) {
                    QueueAchievement(ACH_TOUCHE, 100.0f);
                }
                first = false;
            }
        }
        MoveMatchedTiles();
        ClearSpawnedFlags();
    }
}

// Part of ProcessAttack (port helper): the label reached the health bar. Poison (the larger of
// the current and the incoming), the vampires heal, health drops (not below 0); phase 7.
void Game::AttackHealth() {
    QueueSound(SOUND_DAMAGE);
    AddSimpleLabel(2, &m_incomingDamageLabel.m_position, m_incomingDamage, 0xb, 0.5f);
    if (m_poison < m_incomingPoison) m_poison = m_incomingPoison;
    m_interface.SetPoison(m_poison);
    if (m_incomingPoison > 0) {
        AddSimpleLabel(7, &m_incomingPoisonLabel.m_position, m_incomingPoison, 0xf, 0.5f);
        m_incomingPoison = 0;
    }
    ProcessVampires();
    m_interface.ShowHelpHealth();
    m_attackPhase = 7;
    int health = m_health - m_incomingDamage;
    m_health = health;
    if (health < 0) {
        health = 0;
        m_health = 0;
    }
    m_interface.SetHealth(health);
}

// Game over? Not while health is above 0, except a Sprint challenge that reached turn 100
// (survived). Last Stand saves once: full health instead. At the end: class statistics (games,
// kills, best score per difficulty for normal games, compared unsigned), the score screen entry
// (normal games: XP/gold/upgrade points; Pretzel challenge: chain statistics; Sprint challenge:
// the resource totals / 4), the skills shown with it, the classes saved, the game-over screen,
// the save state emptied (so the game cannot be continued) and the score posted.
bool Game::CheckEndGame() {
    if (m_gameOverScreen.m_active) return true;
    bool survived;
    if (m_reachedTurn100 && m_challenge && m_mode == MODE_SPRINT) {
        survived = true;
        QueueAchievement(ACH_SURVIVOR, 100.0f);
    } else {
        if (m_health > 0) return false;
        RaceType race = m_classes[m_currentClass].m_race;
        if (race != RACE_HUMAN && GetEnemyCount(CharacterClass::GetMortalEnemyType(race)) > 0) {
            QueueAchievement(ACH_NEMESIS, 100.0f);
        }
        if (m_lastStandActive) {
            m_lastStandActive = false;
            m_lastStandActiveNextTurn = false;
            m_health = m_bonusMaxHealth + m_baseMaxHealth;
            m_interface.SetHealth(m_health);
            QueueSound(SOUND_ABILITY);
            GLfloat2 p = Interface::GetPoisonPosition();
            AddSimpleLabel(0x1c, &p, 0, 0x24, 1.0f);
            QueueAchievement(ACH_BACK_FROM_DEAD, 100.0f);
            return false;
        }
        survived = false;
    }
    CharacterClass& cls = m_classes[m_currentClass];
    cls.m_gamesPlayed += 1;
    cls.m_enemiesKilled = m_enemiesKilled + cls.m_enemiesKilled;
    if (!m_challenge) {
        AddScore(m_turn, m_xp + m_xpBanked, m_gold + m_goldBanked, m_upgradePoints + m_upgradePointsBanked, GetScore(),
                 m_difficulty);
        int best = -1;
        if (m_difficulty == DIFF_NORMAL) best = 0;
        else if (m_difficulty == DIFF_HARD) best = 1;
        else if (m_difficulty == DIFF_HARDER) best = 2;
        if (best >= 0) {
            unsigned score = static_cast<unsigned>(GetScore());
            if (cls.m_bestScore[best] < score) cls.m_bestScore[best] = score;
        }
    } else if (m_mode == MODE_PRETZEL) {
        AddScore(m_turn, m_averageChainLength10, m_longestChain, m_chainCount, GetScore(), m_mode);
    } else if (m_mode == MODE_SPRINT) {
        int xp = m_xp + m_xpBanked;
        int gold = m_gold + m_goldBanked;
        int up = m_upgradePoints + m_upgradePointsBanked;
        AddScore(m_turn, xp / 4, gold / 4, up / 4, GetScore(), m_mode);
    }
    int s[4] = {-1, -1, -1, -1};
    for (int i = 0; i < m_skillCount && i < 4; ++i) s[i] = m_skills[i]->m_type;
    SetScoreSkills(s[0], s[1], s[2], s[3]);
    SerialiseClasses();
    ShowGameOver(survived);
    m_saveState.ResetForWriting();   // size 0, changed
    m_postScore = true;
    QueueAchievement(ACH_MISSION_ACCOMPLISHED, 100.0f);
    return true;
}

// The end of a turn (after the enemies' attack): turn count; skills charge (not while a Mage is on
// the board) and lose their one-turn effects; the per-turn flags reset and the "next turn" flags
// take effect; frozen tiles thaw and burning tiles burn their damage; regeneration, then the
// player's poison; the enemies' poison; Werewolf cripples; the special enemies' abilities; Last
// Stand moves on; refill; timers; help pages; the Sprint challenge's turn 100; save. Stops as
// soon as the game is over (CheckEndGame at four points).
void Game::EndOfTurn() {
    m_turn += 1;
    if (CheckEndGame()) return;
    for (int i = 0; i < m_skillCount; ++i) {
        if (GetEnemyCount(ENEMY_MAGE) == 0) {
            Skill* s = m_skills[i];
            if (s->m_cooldown - 1 == s->m_charge) QueueSound(SOUND_ABILITY_READY);
            int charge = s->m_charge + 1;
            s->m_charge = charge <= s->m_cooldown ? charge : s->m_cooldown;
        }
        m_skills[i]->Deactivate();
    }
    m_interface.UpdateSkills();
    m_hackUsedThisTurn = false;
    m_slashUsedThisTurn = false;
    m_coinsTurnedToSkulls = false;
    m_collectRandomRowUsed = false;
    m_collectRandomBlockUsed = false;
    m_collectEdgeTilesUsed = false;
    m_collectBoardUsed = false;
    m_shockUsed = false;
    m_aweUsed = false;
    if (m_runeActive && !m_runeActiveNextTurn) {
        m_arrow.SetColour(0x32, 0xff, 0x32);
        m_runeActive = false;
    }
    m_runeActiveNextTurn = false;
    m_berserkActive = m_berserkActiveNextTurn;
    m_berserkActiveNextTurn = false;
    m_counterActive = false;
    m_doubleDamage = m_doubleDamageNextTurn;
    m_doubleDamageNextTurn = false;
    m_doubleArmour = m_doubleArmourNextTurn;
    m_doubleArmourNextTurn = false;
    m_doubleHealth = m_doubleHealthNextTurn;
    m_doubleHealthNextTurn = false;
    m_doubleGold = m_doubleGoldNextTurn;
    m_doubleGoldNextTurn = false;
    m_stealActive = m_stealActiveNextTurn;
    m_stealActiveNextTurn = false;
    for (int& n : m_chainResourceCounts) n = 0;
    m_boardFlashTime = 0.0f;
    m_boardFlashLength = 1.0f;

    bool shatterSound = false;
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            Tile& t = m_board[row][col];
            if (t.m_frozen) {
                AddFlare(1, m_gridPositions[row][col], GLcolour4{100, 0xe6, 0xff, 0xff});
                t.SetFreezeVisibility(false);
                if (!shatterSound) {
                    QueueSound(SOUND_SHATTER);
                    shatterSound = true;
                }
            } else if (t.m_burning) {
                AddFlare(1, m_gridPositions[row][col], GLcolour4{0xf0, 0x96, 0x0a, 0xff});
                if (t.m_type == TILE_SKULL) {
                    AddSimpleLabel(9, &m_gridPositions[row][col], t.m_burnDamage, 0x12, 1.0f);
                    int health = t.m_health - t.m_burnDamage;
                    if (health < 1) {
                        t.m_health = 0;
                        t.UpdateText(false, false, true);
                        AttackSkull(&t, 0, false);
                        if (m_classes[m_currentClass].m_race == RACE_DRAGONKIN) QueueAchievement(ACH_BURNINATOR, 2.0f);
                    } else {
                        t.m_health = health;
                        t.UpdateText(false, false, true);
                        AddParticles(&t, 2);
                    }
                }
                t.SetFlameVisibility(false, 0, false);
            }
        }
    }

    int maxHealth = m_bonusMaxHealth + m_baseMaxHealth;
    if (m_regeneration > 0 && m_health < maxHealth) {
        QueueSound(SOUND_POTION);
        int health = m_regeneration + m_health;
        m_health = health;
        if (maxHealth < health) {
            m_health = maxHealth;
            health = maxHealth;
        }
        if (m_interface.m_health != health) {
            m_interface.m_health = health;
            m_interface.UpdateHealthBar(Interface::BAR_HEALTH, health);
        }
        AddSimpleLabel(3, m_interface.GetTarget(TILE_POTION), m_regeneration, 0xd, 1.0f);
    }
    if (m_poison > 0) {
        QueueSound(SOUND_POISON);
        int health = m_health - m_poison;
        m_health = health;
        if (health < 0) {
            m_health = 0;
            QueueAchievement(ACH_POISONED, 100.0f);
            health = 0;
        }
        if (m_interface.m_health != health) {
            m_interface.m_health = health;
            m_interface.UpdateHealthBar(Interface::BAR_HEALTH, health);
        }
        GLfloat2 p = Interface::GetPoisonPosition();
        AddSimpleLabel(7, &p, m_poison, 0xf, 1.0f);
        if (CheckEndGame()) return;
    }

    for (int col = 0; col < 6; ++col) {   // poisoned enemies
        for (int row = 0; row < 6; ++row) {
            Tile& t = m_board[row][col];
            if (t.m_type != TILE_SKULL) continue;
            int damage = t.GetPoison();
            if (damage == 0) continue;
            AddSimpleLabel(7, &m_gridPositions[row][col], damage, 0xf, 1.0f);
            int health = t.m_health - damage;
            if (health < 1) {
                MatchTile(&t, TILE_TYPE_COUNT);
                if (m_classes[m_currentClass].m_race == RACE_ELF) QueueAchievement(ACH_FLESH_WOUND, 2.0f);
            } else {
                t.m_health = health;
                t.UpdateText(false, false, true);
                AddParticles(&t, 2);
            }
        }
    }
    for (auto& r : m_board) {
        for (Tile& t : r) t.m_abilityUsed = false;
    }
    ProcessCripples();
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 6; ++row) {
            Tile& t = m_board[row][col];
            if (t.m_type == TILE_SKULL && t.m_enemyType != ENEMY_NORMAL) t.ActivateAbility();
        }
    }
    m_lastStandActivePrevTurn = m_lastStandActive;
    m_lastStandActive = m_lastStandActiveNextTurn;
    m_lastStandActiveNextTurn = false;
    if (CheckEndGame()) return;

    MoveMatchedTiles();
    ClearSpawnedFlags();
    m_trapActive = m_trapActiveNextTurn;
    m_treasureChamberActive = m_treasureChamberActiveNextTurn;
    m_fillingNewBoard = false;
    m_newBoardAvoidsSwords = false;
    m_trapActiveNextTurn = false;
    m_treasureChamberActiveNextTurn = false;
    int timer = m_timeToPromotion - 1;
    m_timeToPromotion = timer < 0 ? 0 : timer;
    // help pages 0xf (skulls) and 0x15 (special enemies): on the topmost such tile of each column
    // (the original moves on to the next column after the first match)
    if (m_helpMenu.m_shownHelp & (1 << 15)) {
        for (int col = 0; col < 6; ++col) {
            for (int row = 0; row < 6; ++row) {
                Tile& t = m_board[row][col];
                if (t.m_type == TILE_SKULL) {
                    ShowHelp(t.m_position.x, t.m_position.y, 0.0828125f, 0.0828125f, 0xf);
                    break;
                }
            }
        }
    }
    if (m_helpMenu.m_shownHelp & (1 << 21)) {
        for (int col = 0; col < 6; ++col) {
            for (int row = 0; row < 6; ++row) {
                Tile& t = m_board[row][col];
                if (t.m_type == TILE_SKULL && t.m_enemyType != ENEMY_NORMAL) {
                    ShowHelp(t.m_position.x, t.m_position.y, 0.0828125f, 0.0828125f, 0x15);
                    break;
                }
            }
        }
    }
    m_chainMadeThisTurn = false;
    m_reachedTurn100 = m_challenge && m_mode == MODE_SPRINT && m_turn == 100;
    if (CheckEndGame()) return;
    SerialiseState();
}
