// Tile (derives from Quad): one cell of the 6x6 board: potion, skull (enemy with stats, labels,
// abilities), coin, shield, sword or special tile. Layout: re/structs/Tile.json (0x270 bytes in
// the original).
#pragma once

#include <list>

#include "dr_types.h"
#include "quad.h"

class Font;
class Game;
class GameState;
class Label;

class Tile : public Quad {
public:
    Tile();                                                      // 0x00063f44
    Tile(const Tile& other) = default;                           // 0x00028800: member-wise, labels shared
    Tile& operator=(const Tile& other) = default;                // 0x00027cd4: member-wise, labels shared
    ~Tile() override;                                            // 0x00063eb8: does not delete the labels

    void InitialiseLabels(Game* game);                           // 0x00063d3c: once (while m_game is null)
    void DeleteLabels();                                         // 0x00063cec
    void SetType(TileType type, int difficultyBase, int difficulty);   // 0x00064ef8
    void SetSpecialType(SpecialTileType type);                   // 0x00065004
    void GenerateStats(int difficultyBase, int difficulty);      // 0x00063098
    void Promote(int difficultyBase, int difficulty, bool byBigGame);  // 0x00064e3c
    void UpdateEnemyUV();                                        // 0x00064b04
    void UpdateText(bool attack, bool armour, bool health);      // 0x00063938
    void UpdateValues();                                         // 0x00063a88
    void UpdateCountdown();                                      // 0x00063030
    void AddHealth(int amount);                                  // 0x00063a5c
    int GetAttack();                                             // 0x00062934
    void AddPoison(int amount);                                  // 0x00062ad4
    int GetPoison();                                             // 0x00062b0c: advances the poison by a turn; returns this turn's poison damage (the old m_poison[0]; the decompiler shows void)
    void ActivateAbility();                                      // 0x00064c78
    int GetDamageType(std::list<Tile*>& chain, std::list<Tile*>::iterator& it);   // 0x000644bc
    bool CanMatchType(std::list<Tile*>& chain, bool ignoreArrows);   // 0x000645ac
    void GetRewards(std::list<TileType>& rewards);               // 0x000646e4
    void MakeMagicSword();                                       // 0x00062db8
    void MakeArrow();                                            // 0x00064294
    void MakeManaPotion();                                       // 0x000642dc
    void MakeAcidPotion();                                       // 0x00064334
    void MakeExplosivePotion();                                  // 0x0006438c
    void MakeBroken();                                           // 0x00064a80
    void SetFlameVisibility(bool visible, int damage, bool fireBreath);   // 0x00062de8
    void SetFreezeVisibility(bool visible);                      // 0x00062e94
    void Dim();                                                  // 0x00062f8c
    void Undim();                                                // 0x00062f14
    bool IsBeingSelected(const GLfloat2& point);                 // 0x00062fec
    void Process(float dt);                                      // 0x000628ec: falling
    void ProcessEffects(float dt);                               // 0x00063b84: glow spin, poison tint
    void Render() override;                                      // 0x000643e4
    void RenderText();                                           // 0x00064458
    void RenderOverlay();                                        // 0x00063b3c
    void RenderCross();                                          // 0x00063b60
    GLfloat2 GetDefenceLabelPosition();                          // 0x00062968
    GLfloat2 GetHealthLabelPosition();                           // 0x000629c8
    GLfloat2 GetAttackLabelPosition();                           // 0x00062a28
    GLfloat2 GetEyePosition(int right);                          // 0x00062a88
    void Serialise(GameState* state);                            // 0x00062c28
    void Deserialise1(GameState* state);                         // 0x00063acc: old format (skull stats only)
    void Deserialise2(GameState* state, int version);            // 0x0006508c

    static void GetEnemyUV(EnemyType type, GLfloat2& topLeft, GLfloat2& bottomRight);   // 0x00062b44
    static const char* GetEnemyName(EnemyType type);             // 0x00062bc8
    static const char* GetEnemyDescription(EnemyType type);      // 0x00062be0
    static void GetMortalGlowUV(GLfloat2& topLeft, GLfloat2& bottomRight);   // 0x00062bf4
    static void GetTileUV(int type, GLfloat2& topLeft, GLfloat2& bottomRight);   // 0x00062d60

    Game* m_game;
    int m_column;
    int m_row;
    GLfloat2 m_baseSize;
    bool m_matched;
    bool m_selected;
    bool m_spawnedThisTurn;          // 0x8a: set by Game::MoveMatchedTiles on new tiles, cleared after the refill
    GLfloat2 m_targetPosition;
    bool m_falling;
    float m_fallSpeed;
    float m_fallAcceleration;
    TileType m_type;
    SpecialTileType m_specialType;
    int m_attack;
    int m_armour;
    int m_health;
    int m_maxHealth;
    float m_renderOffsetX;
    const Font* m_font;
    Label* m_attackLabel;
    Label* m_armourLabel;
    Label* m_healthLabel;
    Label* m_countdownLabel;
    Quad m_cross;
    bool m_crossVisible;
    Quad m_overlay;
    bool m_overlayVisible;
    Quad m_mortalGlow;
    bool m_mortalGlowVisible;
    float m_mortalGlowSpin;
    int m_variant;
    bool m_magicSword;
    bool m_arrow;
    bool m_manaPotion;
    bool m_acidPotion;
    bool m_explosivePotion;
    int m_poison[3];                 // poison damage of this and the next two turns
    bool m_poisonFadeActive;
    bool m_poisonFadeOut;
    float m_poisonFade;
    EnemyType m_enemyType;
    int m_countdown;
    bool m_abilityUsed;
    bool m_frozen;
    bool m_burning;
    bool m_burnIsFireBreath;
    int m_burnDamage;
    bool m_broken;
    bool m_promotedByBigGame;
    bool m_lichJustDied;
    // port (cosmetic, mouse only): a tile added to the chain pops (grows, then settles) and flashes
    bool m_popSelected = false;      // m_selected as ProcessEffects last saw it
    float m_popTime = 0.0f;          // seconds of the pop left
    // port (cosmetic): the idle animation (Game::ProcessIdle sets them, drawing only)
    float m_idleRotation = 0.0f;     // degrees added to the sprite's rotation (the wobble)
    GLfloat2 m_idleOffset{0.0f, 0.0f};   // shift of the tile, its labels and overlays (the shake)

private:
    void RenderPopped();             // port: Quad::Render with the pop's scale and flash, the idle wobble's angle
    void RenderPoppedQuad();
};

extern const GLfloat2 g_leftEyePositions[8];    // 0x00089778 (filled by a static initialiser)
extern const GLfloat2 g_rightEyePositions[8];   // 0x000897b8
