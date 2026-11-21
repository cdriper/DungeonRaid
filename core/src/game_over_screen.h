// GameOverScreen: the end of a game: a red (died) or green (survived) flash with its banner, then
// the summary counting up row by row (turn, level, experience/gold/upgrades or the Pretzel
// numbers, items, skills, perks, flaws, score, the enemies left on the board), then fading out
// (Game::Process then shows the scores). 0x25f4 bytes in the original, embedded in Game at 0xce60;
// the layout below follows it.
#pragma once

#include <vector>

#include "quad.h"

class Game;
class Label;

class GameOverScreen {
public:
    static constexpr int kLabelCount = 22;   // 0..12 headings (name, class, difficulty, rows), 13..21 values
    static constexpr int kEnemyQuads = 36;

    explicit GameOverScreen(Game* game);                // 0x000460dc
    ~GameOverScreen();                                  // 0x00046518
    GameOverScreen(const GameOverScreen&) = delete;
    GameOverScreen& operator=(const GameOverScreen&) = delete;

    void Initialise(bool survived);                     // 0x000457d4
    void InitIcons();                                   // 0x000465e0
    void FadeOut();                                     // 0x000454d0
    void Process(float dt);                             // 0x000469dc
    void Render();                                      // 0x000454f0: also updates the shown counts (as the original)

    Game* m_game;                                       // 0x0
    bool m_active = false;                              // 0x4
    Quad m_shade;                                       // 0x8
    Quad m_loseBanner;                                  // 0x7c
    Quad m_winBanner;                                   // 0xf0
    // (the scalars below are unset by the original's constructor: 0 here)
    bool m_survived = false;                            // 0x164
    int m_state = 0;                                    // 0x168: 0 flash fading, 1 colour, 2 summary, 3 fading out
    int m_stage = 0;                                    // 0x16c: summary row being shown (12 -> 21: the enemies)
    float m_time = 0.0f;                                // 0x170: state progress 0..1 (1 = skip to the end of the row)
    float m_timer = 0.0f;                               // 0x174: until the next row
    float m_value = 0.0f;                               // 0x178: counting number
    float m_speed = 0.0f;                               // 0x17c: its growth factor
    int m_itemsShown = 0;                               // 0x180
    int m_skillsShown = 0;                              // 0x184
    int m_perksShown = 0;                               // 0x188
    int m_flawsShown = 0;                               // 0x18c
    int m_enemiesShown = 0;                             // 0x190
    float m_iconTimer = 0.0f;                           // 0x194
    int m_perkCount = 0;                                // 0x198
    int m_flawCount = 0;                                // 0x19c
    int m_enemyCount = 0;                               // 0x1a0
    std::vector<Label*> m_labels;                       // 0x1a4
    Quad m_perks[4];                                    // 0x1b0
    Quad m_flaws[4];                                    // 0x380
    Quad m_enemies[kEnemyQuads];                        // 0x550
    Quad m_glows[kEnemyQuads];                          // 0x15a0: behind the mortal enemies
    int m_lastTouchState = 0;                           // 0x25f0
};
