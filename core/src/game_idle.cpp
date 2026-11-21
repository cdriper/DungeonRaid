// Game, port-only: the idle animation. After kIdleDelay seconds without a touch or a mouse move on
// the board (between turns, no menu or screen over it), one of two animations plays: a random tile
// wobbles (with its selection sound), or a random row or column shakes. It repeats after another
// kIdleDelay; any touch or mouse move stops it. Cosmetic only: it draws through Tile's m_idle*
// fields and uses its own random numbers, never the game's streams (the replays stay bit-exact).
#include <chrono>
#include <cstdint>

#include "game.h"

namespace {
const float kIdleDelay = 8.0f;          // seconds without input before an animation
// the wobble: angles (degrees; negative = counter-clockwise on screen, y points down) at even steps
const float kWobbleAngles[] = {0.0f, -15.0f, 10.0f, -5.0f, 0.0f};
const int kWobbleSteps = 4;             // segments between the angles above
const float kWobbleStep = 0.15f;        // seconds per segment
// the shake: a new random offset per tile each step, the amplitude falling to 0
const int kShakeSteps = 8;
const float kShakeStep = 0.05f;         // seconds per step
const float kShakeAmplitude = 0.1f;     // of the tile's size

float SmoothStep(float t) { return t * t * (3.0f - 2.0f * t); }
}  // namespace

uint32_t Game::IdleRand() {
    // xorshift32, seeded from the clock on first use
    if (m_idleSeed == 0) {
        m_idleSeed = static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()) | 1u;
    }
    m_idleSeed ^= m_idleSeed << 13;
    m_idleSeed ^= m_idleSeed >> 17;
    m_idleSeed ^= m_idleSeed << 5;
    return m_idleSeed;
}

float Game::IdleRandF(float min, float max) {
    return min + (max - min) * static_cast<float>(IdleRand() >> 8) * (1.0f / 16777216.0f);
}

// The board waits for the player: a game on the board, no menu or screen over it, between turns.
bool Game::IdleAllowed() const {
    return IsPlaying() && !ScreenOverBoard() && m_touchState == NOT_TOUCHING && m_chain.empty() &&
           m_fallingTiles.empty() && m_tokenCount == 0 && m_attackPhase == 0 && m_health > 0;
}

void Game::ResetIdle() {
    m_idleTime = 0.0f;
    if (m_idleKind != IDLE_NONE) StopIdle();
}

void Game::StopIdle() {
    m_idleKind = IDLE_NONE;
    for (auto& row : m_board) {
        for (Tile& tile : row) {
            tile.m_idleRotation = 0.0f;
            tile.m_idleOffset = GLfloat2{0.0f, 0.0f};
        }
    }
}

void Game::StartIdle() {
    StopIdle();
    m_idleTime = 0.0f;
    m_idleAnimTime = 0.0f;
    if (IdleRand() & 0x100) {
        m_idleKind = IDLE_WOBBLE;
        m_idleCell = static_cast<int>(IdleRand() % 36u);
        QueueTileSound(&m_board[m_idleCell / 6][m_idleCell % 6]);
    } else {
        m_idleKind = IDLE_SHAKE;
        m_idleCell = static_cast<int>(IdleRand() % 12u);   // 0..5 a row, 6..11 a column
        m_idleShakeStep = -1;                              // the first frame draws step 0's targets
        for (int i = 0; i < 6; ++i) m_idleShakeTo[i] = GLfloat2{0.0f, 0.0f};
    }
}

// One frame: counts the idle time (reset while the board does not wait), starts and runs the
// animation.
void Game::ProcessIdle() {
    const bool requested = m_idleStartRequested;
    m_idleStartRequested = false;
    if (!IdleAllowed()) {
        ResetIdle();
        return;
    }
    if (requested) {
        StartIdle();
    } else if (m_idleKind == IDLE_NONE) {
        m_idleTime += m_frameTime;
        if (m_idleTime >= kIdleDelay) StartIdle();
        if (m_idleKind == IDLE_NONE) return;
    } else {
        m_idleAnimTime += m_frameTime;
    }
    if (m_idleKind == IDLE_WOBBLE) {
        Tile& tile = m_board[m_idleCell / 6][m_idleCell % 6];
        const float pos = m_idleAnimTime / kWobbleStep;
        const int seg = static_cast<int>(pos);
        if (seg >= kWobbleSteps) {
            StopIdle();
            return;
        }
        const float t = SmoothStep(pos - static_cast<float>(seg));
        tile.m_idleRotation = kWobbleAngles[seg] + (kWobbleAngles[seg + 1] - kWobbleAngles[seg]) * t;
        return;
    }
    // IDLE_SHAKE
    const float pos = m_idleAnimTime / kShakeStep;
    const int step = static_cast<int>(pos);
    if (step >= kShakeSteps) {
        StopIdle();
        return;
    }
    const bool isRow = m_idleCell < 6;
    const int line = isRow ? m_idleCell : m_idleCell - 6;
    auto tileAt = [&](int i) -> Tile& { return isRow ? m_board[line][i] : m_board[i][line]; };
    while (m_idleShakeStep < step) {   // new targets: from where the last step ended
        m_idleShakeStep += 1;
        const float left = 1.0f - static_cast<float>(m_idleShakeStep + 1) / static_cast<float>(kShakeSteps);
        for (int i = 0; i < 6; ++i) {
            Tile& tile = tileAt(i);
            m_idleShakeFrom[i] = m_idleShakeTo[i];
            const float ax = tile.m_size.x * tile.m_scale.x * kShakeAmplitude * left;
            const float ay = tile.m_size.y * tile.m_scale.y * kShakeAmplitude * left;
            m_idleShakeTo[i] = GLfloat2{IdleRandF(-ax, ax), IdleRandF(-ay, ay)};
        }
    }
    const float t = pos - static_cast<float>(step);
    for (int i = 0; i < 6; ++i) {
        const GLfloat2& a = m_idleShakeFrom[i];
        const GLfloat2& b = m_idleShakeTo[i];
        tileAt(i).m_idleOffset = GLfloat2{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
    }
}

// Debug mode: one animation now (Ctrl+A on the desktop). Started by the next frame's ProcessIdle:
// the host calls this between frames, and Process empties the sound queue before it runs.
void Game::DebugIdleAnimation() {
    if (!m_debugMode) return;
    if (!IdleAllowed()) {
        if (InGame()) ShowToast("Debug: not now (only on the board, between turns)");
        return;
    }
    m_idleStartRequested = true;
}
