// Game: the central class (0x15e74 bytes in the original; layout re/structs/Game.json).
//
// Fully ported (game*.cpp).
//
// Fields follow the original's order (offsets in the comments).
#pragma once

#include <list>
#include <set>
#include <vector>

#include "achievements.h"
#include "arrow.h"
#include "character_class.h"
#include "dr_types.h"
#include "font.h"
#include "game_state.h"
#include "interface.h"
#include "label.h"
#include "particles.h"
#include "quad.h"
#include "game_over_screen.h"
#include "help_menu.h"
#include "info_panel.h"
#include "intro_screen.h"
#include "main_menu.h"
#include "name_screen.h"
#include "quad_strip.h"
#include "score_screen.h"
#include "achievements_screen.h"
#include "settings_screen.h"
#include "toast.h"
#include "simple_label.h"
#include "tile.h"
#include "token.h"
#include "unlock_menu.h"

class Item;
class Skill;

class Game {
public:
    explicit Game(float viewWidth = 320.0f, float viewHeight = 480.0f);   // 0x00028c8c (App::init passes the view size)
    ~Game();    // 0x0002c088: the data part

    void Start(bool soundEnabled);          // 0x000260ac: seeds, screens, board geometry, skills, classes
    void SetupRenderer();                   // 0x00026470: viewport (2:3, centred) and GL state
    void SetViewSize(float viewWidth, float viewHeight);   // port-only: a resized window (desktop hosts)
    void Render();                          // 0x00025924: one frame (the screen on top, else the board)
    void BackPressed();                     // 0x00026a98: the Android back key
    void SetFonts(const Font& font, const Font& menuFont, const Font& countFont);   // 0x0002578c (fonts made by Java)
    void CreateFont(const char* font, const char* menuFont, const char* countFont); // 0x000257f8 (.fnt files)
    void ShowBonusButton() {}               // 0x00024750 (empty)
    void ShowMoreGames() {}                 // 0x000249ac (empty)
    void BonusPromptClosed() {}             // 0x00024754 (empty)
    void ShowBonusReceived();               // 0x00025160: the Hack unlock (PlayHaven promo)
    void FinishedLoadingClassesFromAchievements() { SerialiseClasses(); }   // 0x00024de4

    static float s_viewWidth;               // Game::m_viewWidth (0x00068ae0; set by the constructor)
    static float s_viewHeight;              // Game::m_viewHeight (0x00068ae4)

    // --- board
    void CalculateTilePositions();                                     // 0x00025f64
    void PositionTile(Tile* tile, int column, int row, bool animate);  // 0x00024560
    void SwapTiles(int column1, int row1, int column2, int row2);      // 0x00028b14
    Tile* GetTileAtPosition(const GLfloat2& position);                 // 0x00024470
    int GetNearestUnmatchedTile(int column, int row);                  // 0x00024510
    void RandomiseTile(Tile* tile);                                    // 0x0002d584
    int GetTileCount(TileType type);                                   // 0x000246cc
    int GetSpecialEnemyCount();                                        // 0x00024704
    int GetEnemyCount(EnemyType type);                                 // 0x000247b8
    int GetNormalPotionCount();                                        // 0x00024804
    int GetNormalSwordCount();                                         // 0x0002484c
    void AddRevengeHealth();                                           // 0x0002d294
    bool IsTimeToPromote();                                            // 0x000250b0
    void SetTimeToPromotion(int turns);                                // 0x0002985c
    void SetTimeToTrophy(int turns);                                   // 0x00024500
    void InialiseShieldShatter();                                      // 0x0002535c (sic)

    // --- player: stats, items, skills, resources
    void UpdateStats();                                                // 0x00026c94
    Item* GetItem(ItemType type);                                      // 0x00024610
    Item* SetItem(Item* item);                                         // 0x00024644: returns the replaced item
    Skill* SetSkill(Skill* skill);                                     // 0x0002be04: returns the skill object now free
    void RemoveSkill(Skill* skill);                                    // 0x00024eec
    bool HasSkill(int type);                                           // 0x0002468c
    void AddResource(TileType type, int tokenType, int amount);        // 0x00032210 (DungeonRaidTokenType)
    void Healing();                                                    // 0x00028c0c
    int GetScore();                                                    // 0x0002f868
    static const char* GetDifficultyName(int difficulty);              // 0x00024938
    static const char* GetModeName(int mode);                          // 0x00024978

    // --- settings (the game menu's tick boxes)
    void SetSoundState(bool enabled);                                  // 0x00025074
    void SetSoundVolume(int percent);                                  // port-only: 0..100, also SetSoundState(percent > 0)
    void SetAltSelection(bool on);                                     // port-only: mouse click-click chains (the host does it)
    void SetPanelMode(int mode);                                       // port-only: the Info panel setting (PanelMode); the canvas's shape
    void SetGrind(int grind);                                          // port-only: 0 original, 1 fast, 2 faster trophies
    int TrophyKillsLeft() const;                                       // port-only: special kills to the next trophy; 0 none this game, -1 not on Easy
    // port-only (game_debug.cpp): debug mode, the toast, the board check, debug actions
    void SetDebugMode(bool on);
    void ShowToast(const std::string& text);
    bool IsPlaying() const;
    void DebugAddRandomSkill();                                        // a random new skill at level 1, if a slot is free
    void DebugSpawnSpecialEnemy();                                     // a random cell becomes a random special enemy
    bool InGame() const;                                               // a game on the board (menus over it allowed)
    bool DebugCanAct() const;                                          // debug mode, in a game, between turns, no chain
    void DebugIdleAnimation();                                         // one idle animation now (game_idle.cpp)
    void DebugAddAchievement();                                        // the first locked achievement unlocks (game_achievements.cpp)
    void DebugUnlockAll();                                             // every class at level 10: all races, skills, perks
    // port-only (game_achievements.cpp): the achievements' progress (the original kept it on OpenFeint)
    struct AchievementRecord {
        float progress = 0.0f;                                         // percent; 100 = unlocked
        uint32_t time = 0;                                             // Unix seconds of the unlock (0: locked)
    };
    static constexpr size_t kAchievementBytes = 1 + (kAchievementCount - 1) * 5;   // SerialiseAchievementRecords
    bool IsAchievementUnlocked(int achievement) const;
    int UnlockedAchievementCount() const;
    void AddAchievementProgress(int achievement, float amount);        // as the Java shell: + amount, unlock at 100 (toast)
    void ProcessAchievementQueue();                                    // the host, after each frame: drains QueueAchievement's queue
    std::vector<uint8_t> SerialiseAchievementRecords() const;          // count, then percent + time per achievement
    static bool ParseAchievementRecords(const uint8_t* p, size_t size, AchievementRecord* out);   // checks every value
    std::vector<uint8_t> SaveAchievements() const;                     // the "ach" file
    bool LoadAchievements(const std::vector<uint8_t>& bytes);          // false (nothing changed) if damaged
    // port-only (game_idle.cpp): the idle animation (a tile wobbles or a row/column shakes)
    enum IdleKind { IDLE_NONE, IDLE_WOBBLE, IDLE_SHAKE };
    bool IdleAllowed() const;                                          // the board waits for the player
    void ProcessIdle();                                                // one frame (Process)
    void ResetIdle();                                                  // input: restart the wait, stop an animation
    void StartIdle();
    void StopIdle();                                                   // the tiles back in place
    uint32_t IdleRand();                                               // own stream, not the game's
    float IdleRandF(float min, float max);
    void SetHelpState(bool enabled);                                   // 0x0002505c
    void SetClockState(bool enabled);                                  // 0x00025038
    void SetAttackDimmingState(bool enabled);                          // 0x00025010
    void SetHighFramerate(bool enabled);                               // 0x00024fe8
    void UpdateHelpState();                                            // 0x00024fb0

    // --- classes, unlocks, achievements, sounds
    bool IsSkillUnlocked(SkillType skill);                             // 0x000248e8
    bool IsPerkUnlocked(PerkType perk);                                // 0x00024dec
    bool IsRaceUnlocked(RaceType race);                                // 0x000248a0
    void SetClassLevel(ClassType type, int level);                     // 0x00024d28
    void CheckIfAllClassesMaxed();                                     // 0x0002835c
    void QueueAchievement(Achievement achievement, float progress);   // 0x000281cc
    Achievement GetNextAchievement(float& progress);                   // 0x00024780
    void QueueSound(Sound sound);                                      // 0x0002979c
    int GetNextSound();                                                // 0x00024758: -1 when empty

    // --- new game, save states
    void InitNewGame();                                                // 0x0002d7a4
    void SerialiseState();                                             // 0x000265f4
    void DeserialiseState();                                           // 0x00027c8c
    void DeserialiseState1();                                          // 0x0002785c: oldest save format
    void DeserialiseState2(GameState* state, int tileVersion, int version);   // 0x0002733c
    void SerialiseClasses();                                           // 0x00024d98
    void DeserialiseClasses();                                         // 0x00024d54

    // --- board actions of skills and special enemies (game_actions.cpp)
    void Upgrade();                                                    // 0x000251d4
    void ShatterDefence();                                             // 0x00028554
    void CheckShockAwe();                                              // 0x00028318
    void CreateExplosivePotions();                                     // 0x00029df4
    void FreezeSkulls();                                               // 0x00029eb0
    void CreateManaPotions();                                          // 0x00029f74
    void CreateChain();                                                // 0x0002a038
    void CreateArrows();                                               // 0x0002a108
    void CreateMagicSword();                                           // 0x0002a1cc
    void Earthquake();                                                 // 0x0002a2ac
    void DemoteSpecialEnemy();                                         // 0x0002a34c
    void ConvertTiles(TileType from, TileType to);                     // 0x0002a440
    void PromoteNormalEnemy();                                         // 0x0002d454
    void Teleport();                                                   // 0x0002ffb4
    void KillSpecialEnemy();                                           // 0x00030178
    void Awe();                                                        // 0x000302bc
    void Shock();                                                      // 0x00030404
    void CollectRandomBlock();                                         // 0x00030554
    void KillSkullsNextToCoins();                                      // 0x000306fc
    void CollectEdgeTiles();                                           // 0x000308c8
    void CollectBoard();                                               // 0x00030a2c
    void ConvertColumnToSwords();                                      // 0x00030b88
    void CollectRandomRow();                                           // 0x00030c8c
    void GrabTiles(TileType type, TileType as);                        // 0x00030e40
    void ExplodeArmour();                                              // 0x00031050
    void FireBreath();                                                 // 0x00029cfc
    void TeleportEnemy();                                              // 0x00029ad8
    void Stab(Tile* tile);                                             // 0x0002fcd8
    void JumpDown(Tile* tile);                                         // 0x0002d3cc
    void IgniteTiles(Tile* tile);                                      // 0x00029be0
    void ExplodeEnemy(Tile* tile);                                     // 0x0002b2e4
    void Resurrect(Tile* tile);                                        // 0x0002d350
    void MimicDefence(Tile* tile);                                     // 0x0002e84c
    void MakeAcidPotion(Tile* tile);                                   // 0x0002ea40
    void ConvertSwordToSkull(Tile* tile);                              // 0x0002eb4c
    void DestroyUpgrade(Tile* tile);                                   // 0x0002ecb8
    void FleeUp(Tile* tile);                                           // 0x0002eda0
    void LowerSkulls(Tile* tile);                                      // 0x0002f154
    void StabAttack(Tile* tile);                                       // 0x0002f250
    void RunAwayEnemy(Tile* tile);                                     // 0x000300d4
    void AttractShieldsSwords(Tile* tile);                             // 0x0002defc
    void AttractSkulls(Tile* tile);                                    // 0x0002e22c
    void BreakRandomShieldSword(Tile* tile);                           // 0x0002e55c
    void ConvertRandomTiles(Tile* tile);                               // 0x0002e6bc
    void FreezeTiles(Tile* tile);                                      // 0x0002e898
    void ThrowSkull(Tile* tile);                                       // 0x0002eeb0
    void JumpEnemy(Tile* tile);                                        // 0x0002f370
    void HealOrBless(Tile* tile);                                      // 0x00034740
    void HealOrBless(std::set<Tile*>& tiles, bool heal, int count);    // 0x00034614
    void StealGold(Tile* tile);                                        // 0x00034a68
    void StealGold(Tile* thief, std::set<Tile*>& coins, int count);    // 0x000348e8

    // --- collecting, refilling, attacking (game_match.cpp)
    void MatchTile(Tile* tile, TileType as);                           // 0x0002ac40
    void MoveMatchedTiles();                                           // 0x0002fd7c
    bool AttackSkull(Tile* tile, int damage, bool fromChain);          // 0x0002f630: true = killed
    void AddToken(Tile* tile, TileType type, bool bonus, const GLfloat2* position);   // 0x0002a934
    void Shapeshift();                                                 // 0x00024f78
    int CountAttackers();                                              // 0x00024e2c
    void DoCounterattack();                                            // 0x00030f38
    void ProcessVampires();                                            // 0x0002d18c
    void ProcessCripples();                                            // 0x0002f558
    void InitialiseFlares();                                           // 0x0002a548
    void MatchTiles();                                                 // 0x00031bcc: scores the finished chain
    void AddFlare(int type, GLfloat2 position, const GLcolour4& colour);   // 0x00029894 (FlareType)
    void ProcessAttack();                                              // 0x000311b8: the enemies' attack animation, one frame
    // --- effect pools (game_effects.cpp)
    void AddSimpleLabel(Tile* tile, TileType type);                    // 0x00028744: the bonus token label (type 0, colour = resource)
    void AddSimpleLabel(int type, const GLfloat2* position, int value, int colour, float scale);  // 0x0002d0d4 (SimpleLabelType, GLfloat2 const&, ...)
    void AddParticles(Tile* tile, int type);                           // 0x0002838c (ParticleType)
    // --- input and the frame (game_frame.cpp)
    void TouchStart(float x, float y, bool mouse = false);             // 0x0002442c (y scaled by 1.5; port: mouse)
    void TouchDrag(float x, float y);                                  // 0x00024448
    void TouchEnd();                                                   // 0x00024460
    void TouchInterrupted();                                           // 0x00024468
    void PointerMove(float x, float y);                                // port: mouse hover (as TouchDrag's units)
    void PointerLeave();                                               // port
    void MouseWheel(float notches);                                    // port: > 0 scrolls down (the achievements list)
    bool IsBoardScreen() const;                                        // port: the board is what is drawn (tall canvas: at the top)
    bool BoardUnderFade() const;                                       // port: a title / intro screen fading over the board
    // port: the layouts (game_ui.cpp)
    void SetFullscreen(bool on);                                       // the desktop's fullscreen window (the host applies it)
    void TogglePauseMenu();                                            // desktop Esc on the board (game_ui.cpp)
    bool IsWide() const { return m_tallCanvas && m_horizontal; }       // the horizontal (kWideW x kWideH) canvas
    float CanvasAspect() const;                                        // width / height of the canvas (the host's window)
    void CanvasSize(float& w, float& h) const;                         // the canvas in screen units (the board is 1 wide)
    bool HudMenuOpen() const;                                          // the HUD's fold-out, pause or stats menu
    bool ScreenOverBoard() const;                                      // settings, help, unlock or game over
    bool OverlayOpen() const { return HudMenuOpen() || ScreenOverBoard(); }   // a menu or screen over the board
    void MapPointer(float x, float y, float& gx, float& gy) const;     // TouchStart's units -> the game's (as drawn)
    void UpdateTime();                                                 // 0x000258e0
    void CalculateFPS();                                               // 0x00025860
    void QueueTileSound(Tile* tile);                                   // 0x00029800
    static std::set<Tile*>::iterator GetSetIndex(std::set<Tile*>& set, int index);   // 0x000265a8 (no this): begin() if index >= size
    void UpdateDimming();                                              // 0x00025728
    void ProcessShieldShatter();                                       // 0x000252ac
    void ProcessShieldChunk(Quad* chunk, float direction, GLfloat2 base);   // 0x000251e4
    void UpdateCrossVisibility();                                      // 0x0002c5e8
    void ProcessResourceShadows();                                     // 0x000253c4
    void ProcessPretzels();                                            // 0x0002bcd8
    void ProcessTileSelection();                                       // 0x0002c8dc: extends/shortens the chain towards the touch
    void ProcessRune(bool collect);                                    // 0x0002b560
    void ProcessInput();                                               // 0x000330c0: the turn controller and board touches
    void Process();                                                    // 0x000339ac: one frame
    bool ProcessChainLabels();                                         // port helper of Process: the four chain labels; false = the game ended
    bool ProcessLabelFlight(SimpleLabel& label, float& t, float& speed, const GLfloat2& start, float arc);   // port helper: true = arrived
    void ShowScores(int kind, bool byMode) {                           // ScoreScreen::Show(Difficulty|GameMode, true)
        if (byMode) m_scoreScreen.ShowMode(kind, true); else m_scoreScreen.ShowDifficulty(kind, true);
    }
    bool ReleaseChain();                                               // port helper of ProcessInput: false = the game ended
    void ShowUnlock(int classType, int level) { m_unlockMenu.Show(classType, level); }
    void UpdateArrow() { m_arrow.Update(m_chain); }                    // port helper: Arrow::Update(copy of the chain)
    void AddChainTileDamage(Tile* tile);                               // port helper of ProcessTileSelection
    bool CheckEndGame();                                               // 0x0002f960: true = the game is over
    void EndOfTurn();                                                  // 0x0003289c: after the enemies' attack

    void ShowHelp(float x, float y, float w, float h, int help) { m_helpMenu.Initialise(x, y, w, h, help); }
    // port: drawing helpers of Render (game_ui.cpp)
    void RenderBoardScreen();                                          // the board, HUD, effects, info panel
    void RenderOverlays();                                             // the menus and screens over it, the toast
    void RenderScreens();                                              // score / name screens, else board + overlays
    void RenderSideShades(int screenX, int screenW, bool vertical = false);   // the menus' shades beside the centred screen
    void SetBoardProjection();                                         // tall canvas: the board screen below (or above) the panel
    void SetCanvasViewport(float x, float y, float w, float h);        // wide: the viewport for a canvas rectangle
    void SetPartView(float cx, float cy, float gy0, float gy1);        // wide: view for original y gy0..gy1 at canvas (cx, cy)
    void RenderFrame(float x, float y, float w, float h);              // wide: the pause menu's frame around a rectangle
    void RenderFlying();                                               // LAYOUT_SPLIT: the tokens and flying labels, gliding
    static void SetProjection(float left, float right, float bottom, float top);

    // --- screens and menus (scores, class screen, new game / continue / challenge / tutorial)
    void ShowGameOver(bool survived) { m_gameOverScreen.Initialise(survived); }
    void ShowNagScreen();                                              // 0x0002956c
    void AddScore(int turn, int a, int b, int c, int score, Difficulty difficulty) {   // ScoreScreen::AddScore(..., Difficulty)
        m_scoreScreen.AddScore(m_name, turn, a, b, c, score, difficulty);
    }
    void AddScore(int turn, int a, int b, int c, int score, GameMode mode) {           // ScoreScreen::AddScore(..., GameMode)
        m_scoreScreen.AddScore(m_name, turn, a, b, c, score, mode + 4);
    }
    void SetScoreSkills(int s0, int s1, int s2, int s3) { m_scoreScreen.SetSkills(s0, s1, s2, s3); }
    void FadeInMainMenu() { m_mainMenu.FadeIn(); }
    void ShowClassScreen();                                            // ClassScreen::Initialise 0x0003a264 on NameScreen +0x2c7c
    void FadeOutClassScreen();                                         // ClassScreen::FadeOut 0x0003730c
    void ShowScores();                                                 // 0x000251ac: the score page, the main menu inactive
    void ShowBonusRequest();                                           // 0x0002518c
    int GetScoreCount() { return m_scoreScreen.GetScoreCount(); }
    void NewGame();                                                    // 0x0002de68
    void ContinueGame();                                               // 0x0002dec0: only with a saved game
    void StartChallenge();                                             // 0x0002de10
    void StartTutorial();                                              // 0x0002ddb0
    // the special enemies' abilities (Tile::ActivateAbility), each on the acting tile

    // helpers of the board actions (port: code the originals repeat)
    void StartBoardFlash();
    void FinishCollect();
    void ClearSpawnedFlags();
    template <typename Pred>
    Tile* PickTile(int index, Pred pred, int* rowOut, int* colOut);
    Token& NextToken();
    void Explode(Tile* tile, bool kamikaze);
    void KillSpecialEnemyTile(Tile* tile);
    void AttractTiles(Tile* tile, bool skulls);
    void AttackArmour();   // ProcessAttack: the damage label reached the armour
    void AttackHealth();   // ProcessAttack: ... the health

    // --- fields
    float m_lastTime = 0.0f;                           // 0x0
    float m_frameTime = 0.0f;                          // 0x4: seconds since the last frame (UpdateTime)
    float m_fps = 0.0f;                                // 0x8 (CalculateFPS)
    int m_turn = 0;                                    // 0xc
    TouchState m_touchState = NOT_TOUCHING;            // 0x20
    float m_touchX = 0.0f;                             // 0x24
    float m_touchY = 0.0f;                             // 0x28 (screen y * 1.5)
    bool m_touchIsMouse = false;                       // port: the touch is the mouse (live chain arrow, tile pop)
    // port: the tall canvas (hosts; the tests keep the original 2:3): 1 x kTallCanvasHeight (9:16).
    // The board screen sits below the info panel (above it with m_panelAtBottom); the other screens
    // (1 x 1.5) are centred. The screen is m_screenOffsetY down (touches are moved back by it).
    static constexpr float kTallCanvasHeight = 16.0f / 9.0f;
    static constexpr float kPanelHeight = kTallCanvasHeight - 1.5f;   // the info panel's strip
    // m_tallCanvas, m_panelAtBottom and m_horizontal follow the Info panel setting (SetPanelMode).
    enum PanelMode { PANEL_NONE, PANEL_TOP, PANEL_BOTTOM, PANEL_HORIZONTAL };
    int m_panelMode = PANEL_NONE;                      // port: the setting (PANEL_HORIZONTAL: desktop only)
    bool m_panelModeChanged = false;                   // port: for the host, which saves it (and fits the window)
    bool m_tallCanvas = false;                         // a panel: the tall (or the horizontal) canvas
    bool m_panelAtBottom = false;                      // PANEL_BOTTOM; else the panel is above the board (moved down)
    // port: the horizontal layout (desktop setting), three framed cards (the pause menu's frame):
    // on the left the board (the original screen's y kWideBoardTop .. kSplitY); on the right, at
    // the top, the skills bar (y 0 .. kWideSkillsH: the top bar without its separator) above the HUD
    // (y kSplitY .. 1.5, as it is), and below them the info panel (the rest of the height). Each
    // part is drawn from the original screen into its place; flying tokens and labels are drawn
    // once more, gliding from the board to their bar or skill button by their flight progress.
    // The other screens, and the menus over the (still split) board, are the original 1 x 1.5
    // screen centred on the canvas (LAYOUT_FIT).
    enum Layout { LAYOUT_VERTICAL, LAYOUT_SPLIT, LAYOUT_FIT };
    static constexpr float kSplitY = 1.2625f;          // the original HUD's top (Interface::m_bottomPanel)
    static constexpr float kWideBoardTop = 0.246875f;  // the board's top (the top bar's bottom edge region)
    static constexpr float kWideSkillsH = 0.203125f;   // the top bar above its separator (texture rows 438..502)
    static constexpr float kWideHudH = 1.5f - kSplitY;
    static constexpr float kFrameL = 13.0f / 320.0f, kFrameT = 13.0f / 320.0f;   // the frame's sides
    static constexpr float kFrameR = 15.0f / 320.0f, kFrameB = 15.0f / 320.0f;
    static constexpr float kWideColW = kFrameL + 1.0f + kFrameR;                 // a column (card) width
    static constexpr float kWideW = 2.0f * kWideColW;                            // the canvas
    static constexpr float kWideH = kFrameT + (kSplitY - kWideBoardTop) + kFrameB;
    static constexpr float kWideCardH = kFrameT + kWideSkillsH + kWideHudH + kFrameB;   // skills + HUD card
    static constexpr float kWidePanelH = kWideH - kWideCardH - kFrameT - kFrameB;   // the info panel's height
    bool m_desktop = false;                            // set by the host: offers the Horizontal setting, the title screen's Exit
    bool m_canQuit = true;                             // set by the host: false on a web page (no Exit even on a desktop)
    bool m_quitRequested = false;                      // port: the title screen's Exit; the host closes the app
    bool m_horizontal = false;                         // PANEL_HORIZONTAL
    bool m_fullscreen = false;                         // the setting (desktop): the window fills the screen
    bool m_fullscreenChanged = false;                  // for the host, which saves it (the platform host applies it)
    int m_layout = LAYOUT_VERTICAL;                    // this frame's (Render)
    float m_fitLeft = 0.0f, m_fitWidth = 1.0f;         // LAYOUT_FIT: the x range the canvas shows
    bool m_splitPass = false;                          // drawing a column: the flying things wait (RenderFlying)
    Quad m_wideShade;                                  // the menus' shades beside the centred screen (RenderSideShades)
    Quad m_framePiece;                                 // RenderFrame's pieces
    float m_screenOffsetY = 0.0f;
    float m_projectionOffsetY = -1.0f;                 // the offset the projection has (-1: to set)
    bool m_hoverActive = false;                        // port: the mouse is over the window, no button down
    float m_hoverX = 0.0f;                             // port: where (as m_touchX/Y)
    float m_hoverY = 0.0f;
    bool m_arrowLive = false;                          // port: m_arrow holds the live segment to the mouse
    int m_viewportX = 0;                               // 0x10
    int m_viewportY = 0;                               // 0x14
    int m_viewportWidth = 0;                           // 0x18
    int m_viewportHeight = 0;                          // 0x1c
    Font m_fonts[3];                                   // 0x2c
    Arrow m_arrow;                                     // 0x3668
    Interface m_interface;                             // 0x4cec
    int m_inputReady = 0;                              // 0x5f74 (a byte in the original; InitNewGame: 1): 0 after a chain until the enemies' turn and EndOfTurn are done
    Label m_fpsLabel;                                  // 0x5f78
    float m_tileSize = 0.0f;                           // 0x5fc4 (Start: 0.165625)
    float m_halfTileSize = 0.0f;                       // 0x5fc8 (Start: 0.0828125)
    Tile m_board[6][6];                                // 0x5fcc [row][column]
    std::list<Tile*> m_chain;                          // 0xb78c
    std::set<Tile*> m_chainSet;                        // 0xb794 (meaning not settled)
    std::list<Tile*> m_fallingTiles;                   // 0xb7ac
    GLfloat2 m_gridPositions[6][6] = {};               // 0xb7b4 [row][column]
    std::vector<Token> m_tokens;                       // 0xb8d4 (reused up to m_tokenCount)
    int m_tokenCount = 0;                              // 0xb8e0
    std::vector<Particles> m_particles;                // 0xb8e4 (reused up to m_particleCount)
    int m_particleCount = 0;                           // 0xb8f0
    std::vector<SimpleLabel> m_simpleLabels;           // 0xb8f4 (reused up to m_simpleLabelCount)
    int m_simpleLabelCount = 0;                        // 0xb900
    SimpleLabel m_simpleLabelB904;                     // 0xb904 (Start: colour 10, lifetime 0.2)
    // The enemies' attack (ProcessAttack): 0 none, 1 shade fades in, 2 eye flares, 3 counterattack
    // banner, 4 incoming damage banner, 5 damage label flies to the armour, 6 ... to the health,
    // 7 fade out, 8 done (ProcessInput starts and ends it)
    int m_attackPhase = 0;                             // 0xbc5c
    Quad m_attackBanner;                               // 0xbc60: red banner behind the incoming damage
    Quad m_attackShade;                                // 0xbcd4: darkens the screen
    Quad m_counterBanner;                              // 0xbd48: yellow banner of a counterattack
    SimpleLabel m_incomingDamageLabel;                 // 0xbdbc
    float m_incomingLabelY = 0.0f;                     // 0xc114 (Start: 0.6675)
    float m_attackFade = 0.0f;                         // 0xc118: banner fade-in 0..1
    GLfloat2 m_damageLabelTarget{0.0f, 0.0f};          // 0xc11c: where the damage label flies (phases 5, 6)
    GLfloat2 m_damageLabelStart{0.0f, 0.0f};           // 0xc124
    float m_damageLabelT = 0.0f;                       // 0xc12c: flight progress 0..1
    float m_damageLabelSpeed = 0.0f;                   // 0xc130
    float m_damageLabelStartScale = 0.0f;              // 0xc134
    float m_damageLabelEndScale = 0.0f;                // 0xc138
    float m_damageLabelArc = 0.0f;                     // 0xc13c: height of the flight's arc
    std::vector<Quad> m_flares;                        // 0xc140 (reused up to m_flareCount)
    int m_flareCount = 0;                              // 0xc14c
    float m_boardFlashTime = 0.0f;                     // 0xc150 (board actions: 0; also the flares' scale in ProcessAttack)
    float m_boardFlashLength = 0.0f;                   // 0xc154 (board actions: 1.0; also the flares' fade in ProcessAttack)
    int m_health = 0;                                  // 0xc158
    int m_baseMaxHealth = 0;                           // 0xc15c
    int m_bonusMaxHealth = 0;                          // 0xc160
    int m_armour = 0;                                  // 0xc164
    int m_maxArmour = 0;                               // 0xc168
    int m_xp = 0;                                      // 0xc170
    int m_upgradePoints = 0;                           // 0xc174
    int m_gold = 0;                                    // 0xc178
    int m_weaponDamage = 0;                            // 0xc17c
    int m_baseDamage = 0;                              // 0xc180
    int m_bonusDamage = 0;                             // 0xc184
    int m_armourDamageChance = 0;                      // 0xc188
    int m_armourPiercingChance = 0;                    // 0xc18c
    float m_xpFraction = 0.0f;                         // 0xc190
    float m_xpBonusRate = 0.0f;                        // 0xc194
    float m_goldFraction = 0.0f;                       // 0xc198
    float m_goldBonusRate = 0.0f;                      // 0xc19c
    float m_upgradeFraction = 0.0f;                    // 0xc1a0
    float m_upgradeBonusRate = 0.0f;                   // 0xc1a4
    int m_strength = 0;                                // 0xc1a8
    int m_itemStrength = 0;                            // 0xc1ac
    int m_dexterity = 0;                               // 0xc1b0
    int m_itemDexterity = 0;                           // 0xc1b4
    int m_vitality = 0;                                // 0xc1b8
    int m_itemVitality = 0;                            // 0xc1bc
    int m_luck = 0;                                    // 0xc1c0
    int m_itemLuck = 0;                                // 0xc1c4
    int m_baseHealAmount = 0;                          // 0xc1c8
    int m_bonusHealAmount = 0;                         // 0xc1cc
    int m_baseArmourAmount = 0;                        // 0xc1d0
    int m_bonusArmourAmount = 0;                       // 0xc1d4
    int m_baseCoinTokenChance = 0;                     // 0xc1d8
    int m_bonusCoinTokenChance = 0;                    // 0xc1dc
    int m_basePotionTokenChance = 0;                   // 0xc1e0
    int m_bonusPotionTokenChance = 0;                  // 0xc1e4
    int m_baseSkullTokenChance = 0;                    // 0xc1e8
    int m_bonusSkullTokenChance = 0;                   // 0xc1ec
    int m_baseShieldTokenChance = 0;                   // 0xc1f0
    int m_bonusShieldTokenChance = 0;                  // 0xc1f4
    char m_name[17] = {};                              // 0xc1f8
    bool m_nameChanged = false;                        // 0xc209
    int m_level = 0;                                   // 0xc20c
    int m_xpBanked = 0;                                // 0xc210
    int m_goldBanked = 0;                              // 0xc214
    int m_upgradePointsBanked = 0;                     // 0xc218
    int m_poisonAttack = 0;                            // 0xc21c
    int m_lifeLeech = 0;                               // 0xc220
    bool m_quicken = false;                            // 0xc224
    int m_counterDamage = 0;                           // 0xc228
    int m_attackReduction = 0;                         // 0xc22c
    int m_regeneration = 0;                            // 0xc230
    bool m_doubleDamage = false;                       // 0xc234
    bool m_doubleArmour = false;                       // 0xc235
    bool m_doubleHealth = false;                       // 0xc236
    bool m_doubleGold = false;                         // 0xc237
    bool m_doubleDamageNextTurn = false;               // 0xc238
    bool m_doubleArmourNextTurn = false;               // 0xc239
    bool m_doubleHealthNextTurn = false;               // 0xc23a
    bool m_doubleGoldNextTurn = false;                 // 0xc23b
    int m_chainDamage = 0;                             // 0xc23c
    bool m_chainHasSkull = false;                      // 0xc240: the player's skull damage is in m_chainDamage (once per chain)
    int m_incomingDamage = 0;                          // 0xc244
    Item* m_items[5] = {};                             // 0xc248 by ItemType
    std::vector<Item**> m_itemSlots;                   // 0xc25c: &m_items[0..4]
    Quad m_shieldShatterLeft;                          // 0xc268
    Quad m_shieldShatterRight;                         // 0xc2dc
    bool m_shieldShatterActive = false;                // 0xc350
    float m_shieldShatterTime = 0.0f;                  // 0xc354
    Skill* m_skills[4] = {};                           // 0xc358 (owned; the first m_skillCount are in use)
    int m_skillCount = 0;                              // 0xc368
    int m_chainResourceCounts[5] = {};                 // 0xc36c
    bool m_fillingNewBoard = false;                    // 0xc380
    bool m_newBoardAvoidsSwords = false;               // 0xc381
    bool m_counterActive = false;                      // 0xc382
    bool m_stealActive = false;                        // 0xc383
    bool m_stealActiveNextTurn = false;                // 0xc384
    SimpleLabel m_simpleLabelC388;                     // 0xc388 (Start: colour 0x11, lifetime 0.2)
    int m_lifeLeechHeal = 0;                           // 0xc6e0
    // the flights of the life leech (0xc6e4), burn (0x146a4) and Spiky (0x14a18) labels after a chain
    float m_leechLabelArc = 0.0f;                      // 0xc6e4
    GLfloat2 m_leechLabelStart{0.0f, 0.0f};            // 0xc6e8
    float m_leechLabelT = 0.0f;                        // 0xc6f0
    float m_leechLabelSpeed = 0.0f;                    // 0xc6f4
    IntroScreen m_introScreen;                         // 0xc6f8
    MainMenu m_mainMenu;                               // 0xc7dc (its constructor opens it)
    GameOverScreen m_gameOverScreen;                   // 0xce60
    ScoreScreen m_scoreScreen;                         // 0xf454
    std::vector<Sound> m_soundQueue;                   // 0x142cc (reused up to m_soundCount)
    int m_soundCount = 0;                              // 0x142d8
    int m_soundRead = 0;                               // 0x142dc
    struct QueuedAchievement {
        Achievement achievement;
        float progress;
    };
    std::vector<QueuedAchievement> m_achievementQueue; // 0x142e0 (reused up to m_achievementCount)
    int m_achievementCount = 0;                        // 0x142ec
    int m_achievementRead = 0;                         // 0x142f0
    bool m_explosionSoundPlayed = false;               // 0x142f4
    bool m_poisonSoundPlayed = false;                  // 0x142f5
    bool m_soundEnabled = false;                       // 0x142f6
    bool m_soundStateChanged = false;                  // 0x142f7
    bool m_helpEnabled = false;                        // 0x142f8
    bool m_clockEnabled = false;                       // 0x142f9
    bool m_attackDimmingEnabled = false;               // 0x142fa
    bool m_highFramerate = false;                      // 0x142fb
    bool m_clockStateChanged = false;                  // 0x142fc
    bool m_dimmingStateChanged = false;                // 0x142fd
    bool m_highFramerateStateChanged = false;          // 0x142fe
    GameState m_saveState{0x800};                      // 0x14300
    GameState m_classState{0x400};                     // 0x14314
    Difficulty m_difficulty = DIFF_NORMAL;             // 0x14328
    bool m_difficultyChanged = false;                  // 0x1432c
    GameMode m_mode = MODE_SPRINT;                     // 0x14330
    bool m_modeChanged = false;                        // 0x14334
    bool m_challenge = false;                          // 0x14335
    int m_timeToPromotion = 0;                         // 0x14338
    bool m_promotionTimerEnabled = false;              // 0x1433c
    bool m_trapActive = false;                         // 0x1433d
    bool m_treasureChamberActive = false;              // 0x1433e
    bool m_trapActiveNextTurn = false;                 // 0x1433f
    bool m_treasureChamberActiveNextTurn = false;      // 0x14340
    SimpleLabel m_simpleLabel14344;                    // 0x14344 (Start: colour 0x12, lifetime 0.2)
    int m_chainBurnDamage = 0;                         // 0x1469c
    int m_keepChainBurnDamage = 0;                     // 0x146a0 (InitNewGame: 0): read as a byte; set keeps m_chainBurnDamage when a chain is dropped
    float m_burnLabelArc = 0.0f;                       // 0x146a4
    GLfloat2 m_burnLabelStart{0.0f, 0.0f};             // 0x146a8
    float m_burnLabelT = 0.0f;                         // 0x146b0
    float m_burnLabelSpeed = 0.0f;                     // 0x146b4
    SimpleLabel m_simpleLabel146b8;                    // 0x146b8 (Start: colour 0x13, lifetime 0.2)
    int m_spikyDamage = 0;                             // 0x14a10 (InitNewGame: 0): the chain damage while the chain holds a Spiky enemy, else 0
    bool m_chainHasSpiky = false;                      // 0x14a14 (InitNewGame: 0)
    bool m_keepSpikyDamage = false;                    // 0x14a15 (InitNewGame: 0): set keeps m_spikyDamage when a chain is dropped
    float m_spikyLabelArc = 0.0f;                      // 0x14a18
    GLfloat2 m_spikyLabelStart{0.0f, 0.0f};            // 0x14a1c
    float m_spikyLabelT = 0.0f;                        // 0x14a24
    float m_spikyLabelSpeed = 0.0f;                    // 0x14a28
    SimpleLabel m_incomingPoisonLabel;                 // 0x14a2c
    int m_incomingPoison = 0;                          // 0x14d84
    int m_poison = 0;                                  // 0x14d88
    bool m_showFeint = false;                          // 0x14d8c: the Java shell shows Feint (JNI Game_ShouldShowFeint)
    bool m_postScore = false;                          // 0x14d8d: the Java shell posts the score (JNI Game_ShouldPostScore)
    int m_chainKills = 0;                              // 0x14d90
    bool m_chainMadeThisTurn = false;                  // 0x14d94
    bool m_showNagScreen = false;                      // 0x14d95: the Java shell shows the nag screen (JNI Game_ShouldShowNagScreen)
    int m_nagScreenCount = 0;                          // 0x14d98 (JNI Get/SetNagScreenCount)
    bool m_nagScreenCountChanged = false;              // 0x14d9c
    bool m_timeToTrophyChanged = false;                // 0x14d9d
    int m_timeToTrophy = 0;                            // 0x14da0
    int m_trophiesSpawned = 0;                         // 0x14da4
    bool m_hackUsedThisTurn = false;                   // 0x14da9
    bool m_slashUsedThisTurn = false;                  // 0x14daa
    bool m_collectingBoard = false;                    // 0x14dab
    CharacterClass m_classes[10];                      // 0x14dac
    int m_currentClass = 0;                            // 0x15cd4
    bool m_currentClassChanged = false;                // 0x15cd8
    bool m_runeActive = false;                         // 0x15cd9
    bool m_runeActiveNextTurn = false;                 // 0x15cda
    std::vector<QuadStrip> m_runeMarkers;              // 0x15cdc: the runes on enclosed tiles (ProcessRune)
    int m_runeMarkerCount = 0;                         // 0x15ce8
    bool m_lastStandActive = false;                    // 0x15cec
    bool m_lastStandActiveNextTurn = false;            // 0x15ced
    bool m_lastStandActivePrevTurn = false;            // 0x15cee
    bool m_berserkActive = false;                      // 0x15cef
    bool m_berserkActiveNextTurn = false;              // 0x15cf0
    int m_chainKilledAttack = 0;                       // 0x15cf4
    // what the chain being built would give (ProcessResourceShadows; the counters are not reset there)
    int m_shadowPotions = 0;                           // 0x15cf8: plain potions
    int m_shadowKills = 0;                             // 0x15cfc: skulls the chain would kill (cross shown)
    int m_shadowCoins = 0;                             // 0x15d00
    int m_shadowShields = 0;                           // 0x15d04: unbroken shields; then the shields beyond full armour
    int m_shadowSwords = 0;                            // 0x15d08 (ProcessRune: enclosed swords)
    int m_shadowOthers = 0;                            // 0x15d0c (ProcessRune: enclosed tiles of other types)
    std::vector<SimpleLabel> m_crossLabels;            // 0x15d10: 'Uppercut'/'Blocked' labels of UpdateCrossVisibility
    int m_crossLabelCount = 0;                         // 0x15d1c (InitNewGame: 0)
    UnlockMenu m_unlockMenu;                           // 0x15d20
    HelpMenu m_helpMenu;                               // 0x103a0
    NameScreen m_nameScreen;                           // 0x103e4 (holds ClassScreen and ModeScreen)
    int m_enemiesKilled = 0;                           // 0x15e14
    bool m_coinsTurnedToSkulls = false;                // 0x15e18
    bool m_collectRandomRowUsed = false;               // 0x15e19
    bool m_collectRandomBlockUsed = false;             // 0x15e1a
    bool m_collectEdgeTilesUsed = false;               // 0x15e1b
    bool m_collectBoardUsed = false;                   // 0x15e1c
    int m_tilesSpawned = 0;                            // 0x15e20
    int m_skullsSpawned = 0;                           // 0x15e24
    float m_boardOffsetY = 0.0f;                       // 0x15e28 (challenges: 0.015625)
    int m_pretzelScore = 0;                            // 0x15e2c
    int m_scoreGain = 0;                               // 0x15e30
    int m_scoreBonus = 0;                              // 0x15e34
    int m_chainPotionTokens = 0;                       // 0x15e38 (InitNewGame: 0; next to the other per-type counters)
    int m_chainSkullTokens = 0;                        // 0x15e3c
    int m_chainCoinTokens = 0;                         // 0x15e40
    int m_chainShieldTokens = 0;                       // 0x15e44
    int m_chainSwordTokens = 0;                        // 0x15e48 (InitNewGame: 0)
    int m_chainBonusPotionTokens = 0;                  // 0x15e4c (InitNewGame: 0)
    int m_chainBonusSkullTokens = 0;                   // 0x15e50
    int m_chainBonusCoinTokens = 0;                    // 0x15e54
    int m_chainBonusShieldTokens = 0;                  // 0x15e58
    int m_chainBonusSwordTokens = 0;                   // 0x15e5c (InitNewGame: 0)
    int m_chainCount = 0;                              // 0x15e60
    int m_averageChainLength10 = 0;                    // 0x15e64
    int m_longestChain = 0;                            // 0x15e68
    int m_scorePage = 1;                               // 0x15e6c
    bool m_scorePageChanged = false;                   // 0x15e70
    bool m_reachedTurn100 = false;                     // 0x15e71
    bool m_shockUsed = false;                          // 0x15e72
    bool m_aweUsed = false;                            // 0x15e73
    SettingsScreen m_settingsScreen;                   // port-only: the settings (title screen, pause menu)
    AchievementsScreen m_achievementsScreen;           // port-only: the achievements list (title screen)
    InfoPanel m_infoPanel;                             // port-only: below the HUD on the tall canvas
    int m_soundVolume = 100;                           // port-only: 0..100, 0 = sound off (the settings slider)
    bool m_soundVolumeChanged = false;                 // port-only: for the host, which applies and saves it
    bool m_debugMode = false;                          // port-only: three taps on the build info; saved by the host
    bool m_debugModeChanged = false;                   // port-only: for the host
    int m_debugPendingMenu = -1;                       // port-only: debug page: fold-out menu to open once no menu is open (0 level-up, 1 shop, 2 upgrade)
    bool m_mouseAvailable = false;                     // port-only: set by the host; shows the Alt selection setting
    bool m_altSelection = false;                       // port-only: a mouse click starts a chain, the next ends it, right click cancels
    bool m_altSelectionChanged = false;                // port-only: for the host, which saves it
    int m_grind = 0;                                   // port-only: the trophy rate (KillSpecialEnemyTile): 0 original, 1 fast, 2 faster
    bool m_grindChanged = false;                       // port-only: for the host, which saves it
    // port-only: the clipboard, done by the host after the frame (progress export/import, settings)
    std::string m_clipboardOut;                        // the text to copy when m_clipboardWrite
    bool m_clipboardWrite = false;
    bool m_clipboardReadRequested = false;             // the host hands the text to SettingsScreen::ReceiveImport
    Toast m_toast;                                     // port-only: short messages over everything
    // port-only: the idle animation (game_idle.cpp)
    float m_idleTime = 0.0f;                           // seconds the board has waited without input
    IdleKind m_idleKind = IDLE_NONE;                   // the animation playing
    float m_idleAnimTime = 0.0f;                       // seconds into it
    int m_idleCell = 0;                                // wobble: row * 6 + column; shake: 0..5 a row, 6..11 a column
    int m_idleShakeStep = -1;                          // shake: the step whose targets are set
    GLfloat2 m_idleShakeFrom[6] = {};                  // shake: each tile's offset at the step's start
    GLfloat2 m_idleShakeTo[6] = {};                    // and at its end
    uint32_t m_idleSeed = 0;                           // IdleRand's state (0: not seeded yet)
    bool m_idleStartRequested = false;                 // debug: start one on the next frame
    // port-only: the achievements (game_achievements.cpp)
    AchievementRecord m_achievementRecords[kAchievementCount];   // by Achievement (entry 0 unused)
    bool m_achievementsChanged = false;                // for the host, which saves the "ach" file
};
