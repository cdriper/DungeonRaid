// Game, port-only: debug mode (toggled by three taps on the title screen's build info, saved by the
// host) and its actions. Nothing here runs unless debug mode is on.
#include <vector>

#include "game.h"
#include "rand.h"
#include "skill.h"

void Game::SetDebugMode(bool on) {
    m_debugModeChanged = m_debugModeChanged || on != m_debugMode;
    m_debugMode = on;
}

void Game::ShowToast(const std::string& text) { m_toast.Show(text); }

// The board (no title, name, intro, score or game-over screen, no settings, no HUD menu open).
bool Game::IsPlaying() const {
    return !m_mainMenu.m_active && !m_nameScreen.m_active && !m_introScreen.m_active && !m_scoreScreen.m_active &&
           !m_gameOverScreen.m_active && !m_settingsScreen.m_active && !m_interface.m_foldoutMenu.m_active &&
           !m_interface.m_gameMenu.m_active && !m_interface.m_statsMenu.m_active;
}

// A game is on the board: no title, name, intro, score or game-over screen (the settings, the
// pause menu or the debug page may be over it).
bool Game::InGame() const {
    return !m_mainMenu.m_active && !m_nameScreen.m_active && !m_introScreen.m_active && !m_scoreScreen.m_active &&
           !m_gameOverScreen.m_active;
}

// Debug actions change the board or open dialogs: only between turns (no chain, nothing falling or
// flying, not during the enemies' attack) and not over a level-up/shop/upgrade dialog.
bool Game::DebugCanAct() const {
    return m_debugMode && InGame() && !m_interface.m_foldoutMenu.m_active && m_chain.empty() &&
           m_fallingTiles.empty() && m_tokenCount == 0 && m_attackPhase == 0;
}

// The whole meta progression: every class (the Adventurer stays at level 1, as always) at level
// 10, which unlocks all races (level 5), skills (level 10) and perk slots; saved as the classes are.
// No achievements are awarded (the debug page's Add achievement does those).
void Game::DebugUnlockAll() {
    if (!m_debugMode) return;
    for (int c = 0; c < 10; ++c) {
        if (m_classes[c].m_class != CLASS_ADVENTURER) m_classes[c].SetLevel(10);
    }
    SerialiseClasses();   // marks the state changed: the host writes cla
    m_nameScreen.SelectClass(m_classes[m_currentClass]);
    ShowToast("Debug: all classes at level 10");
}

// A random cell (cosmetic stream) becomes a skull and is promoted to a random special enemy, as
// Big Game promotes one (the promotion timer kept). Promote draws from the game's tile stream, so
// the rest of the game changes; debug mode only.
void Game::DebugSpawnSpecialEnemy() {
    if (!DebugCanAct()) {
        ShowToast("Debug: not now (only on the board, between turns)");
        return;
    }
    const int cell = RandI(0, 35, RAND_COSMETIC);
    const int row = cell / 6, col = cell % 6;
    Tile& t = m_board[row][col];
    const bool timer = m_promotionTimerEnabled;
    m_promotionTimerEnabled = false;
    if (t.m_type != TILE_SKULL) t.SetType(TILE_SKULL, m_turn, m_difficulty);
    t.Promote(m_turn, m_difficulty, false);
    m_promotionTimerEnabled = timer;
    AddFlare(1, m_gridPositions[row][col], GLcolour4{0xf0, 0x96, 10, 0xff});
    StartBoardFlash();
    ShowToast(std::string("Debug: added ") + Tile::GetEnemyName(t.m_enemyType));
}

// A skill the character does not have yet, at level 1, in the next free slot (as a level-up choice
// puts it there). The pick uses the cosmetic random stream, not the game's.
void Game::DebugAddRandomSkill() {
    if (!m_debugMode) return;
    if (!DebugCanAct()) {
        if (InGame()) ShowToast("Debug: not now (only on the board, between turns)");
        return;
    }
    if (m_skillCount >= 4) {
        ShowToast("All 4 skill slots are full");
        return;
    }
    std::vector<SkillType> candidates;
    for (int t = 0; t < SKILL_COUNT; ++t) {
        if (!HasSkill(t)) candidates.push_back(static_cast<SkillType>(t));
    }
    if (candidates.empty()) return;
    const SkillType type = candidates[static_cast<size_t>(RandI(0, static_cast<int>(candidates.size()) - 1, RAND_COSMETIC))];
    Skill* skill = new Skill(this);
    skill->Initialise(type, nullptr);
    Skill* freed = SetSkill(skill);   // the slot's previous (unused) object, or `skill` if no slot
    if (freed == skill) {
        delete skill;
        ShowToast("All 4 skill slots are full");
        return;
    }
    delete freed;
    ShowToast(std::string("Debug: added skill ") + Skill::GetSkillName(type));
}
