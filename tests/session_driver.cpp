// Native session driver for the end-to-end check (slice 3c): plays a whole game on the port the way
// harness/session.py plays the original in the emulator (boot like the Java shell, frames with a
// fake clock, touches on grid cells and on the fold-out menu's buttons), so harness/check.py --port
// can replay the recorded scripts and compare every snapshot with harness/data/expected/.
//
// The fold-out menu (level-up, shop, upgrade) is ported except its labels. The screens, the other
// menus and most of the HUD are not ported yet. Their hooks do what the original's code does to the
// state a replay depends on, and nothing else:
//   - HelpMenu::Initialise only acts on pending help pages (mask bit set); that is logged as an
//     unsupported event (the recorded games have help off: mask 0).
//   - UnlockMenu::Show, and any call of a board-action hook (all ported, so never expected), are
//     logged as unsupported events.
//   - Everything else (the screens' Process, the HUD's input/heartbeat/tooltip, bars, the menu's
//     labels, Arrow::Update, scores) is a no-op: cosmetic, or only reached from screens and buttons a
//     replay does not use.
// Game::Render is not called (it writes no game state; App::render only draws).
#include <cstdint>
#include <cstring>
#include <string>

#include "game.h"
#include "game_state.h"
#include "interface.h"
#include "rand.h"
#include "skill.h"
#include "util.h"

// C entry points (the old repo loaded this file as a DLL from Python; here golden_replay links it).
#define DRTEST_API extern "C"

namespace {
constexpr int32_t kFrameMicros = 33333;   // one frame at 30 fps (session.py FRAME_US)

struct Session {
    Game* game = nullptr;
    std::string events;         // unsupported hook calls, one per line
};

Session* g_session = nullptr;   // the hooks are global; one session at a time
int32_t g_clock = 0;
uint32_t g_time = 0;

int32_t SessionClock() { return g_clock; }
uint32_t SessionTime() { return g_time; }

void Event(const std::string& s) {
    if (g_session) g_session->events += s + "\n";
}

}  // namespace

// New App, App::init(320, 480), Game::Start(false) with time() = seed and the clock at 0.
DRTEST_API Session* dr_Session_New(uint32_t seed) {
    delete g_session;
    g_session = new Session();
    g_time = seed;
    g_clock = 0;
    SetTimeSource(SessionTime);
    SetClockSource(SessionClock);
    g_session->game = new Game();
    g_session->game->Start(false);
    return g_session;
}

DRTEST_API void dr_Session_Delete(Session* s) {
    if (!s) return;
    delete s->game;
    if (s == g_session) g_session = nullptr;
    delete s;
}

// n frames: the clock advances one frame, then App::process (Game::Process).
DRTEST_API void dr_Session_Frames(Session* s, int n) {
    for (int i = 0; i < n; ++i) {
        g_clock += kFrameMicros;
        s->game->Process();
    }
}

// The new-game setup the scripts may give (class, challenge, mode, difficulty), before NewGame.
DRTEST_API void dr_Session_Setup(Session* s, const char* key, int value) {
    Game* g = s->game;
    if (std::strcmp(key, "class") == 0) {
        g->m_currentClass = value;
    } else if (std::strcmp(key, "challenge") == 0) {
        g->m_challenge = value != 0;
    } else if (std::strcmp(key, "mode") == 0) {
        g->m_mode = static_cast<GameMode>(value);
    } else if (std::strcmp(key, "difficulty") == 0) {
        g->m_difficulty = static_cast<Difficulty>(value);
    } else {
        Event(std::string("unknown setup key ") + key);
    }
}

// Like session.py tap_point: touch at game-space (x, y) (y divided by 1.5 in double), hold, release.
DRTEST_API void dr_Session_TapPoint(Session* s, double x, double y, int hold, int after) {
    s->game->TouchStart(static_cast<float>(x), static_cast<float>(y / 1.5));
    dr_Session_Frames(s, hold);
    s->game->TouchEnd();
    dr_Session_Frames(s, after);
}

// (type, charge, cooldown, active) of the skills in use; returns their count.
DRTEST_API int dr_Session_Skills(Session* s, int* out) {
    Game* g = s->game;
    for (int i = 0; i < g->m_skillCount; ++i) {
        const Skill* k = g->m_skills[i];
        out[4 * i] = k->m_type;
        out[4 * i + 1] = k->m_charge;
        out[4 * i + 2] = k->m_cooldown;
        out[4 * i + 3] = k->m_active ? 1 : 0;
    }
    return g->m_skillCount;
}

// Game::InitNewGame, then MainMenu::SetActive(false) (its active byte, Game 0xc7e0).
DRTEST_API void dr_Session_NewGame(Session* s) {
    s->game->InitNewGame();
    s->game->m_mainMenu.m_active = false;
}

// kind 0 = TouchStart, 1 = TouchDrag at the centre of grid cell (row, col), 2 = TouchEnd.
// Like session.py: the cell position in game space (1.0 x 1.5), y divided by 1.5 in double.
DRTEST_API void dr_Session_Touch(Session* s, int kind, int row, int col) {
    Game* g = s->game;
    if (kind == 2) {
        g->TouchEnd();
        return;
    }
    const GLfloat2& p = g->m_gridPositions[row][col];
    float x = p.x;
    float y = static_cast<float>(static_cast<double>(p.y) / 1.5);
    if (kind == 0) {
        g->TouchStart(x, y);
    } else {
        g->TouchDrag(x, y);
    }
}

// The fold-out menu's last touch state (session.py's "busy": ProcessInput only reacts to a new press).
DRTEST_API int dr_Session_MenuBusy(Session* s) { return s->game->m_interface.m_foldoutMenu.m_lastTouchState; }

// Like session.py tap_point(menu_quad_center(q)): the quad's centre (vertices 0 and 2) plus the menu
// offset, computed in double from the floats; touch there (y divided by 1.5 in double), `hold`
// frames, release, `after` frames.
DRTEST_API void dr_Session_TapQuad(Session* s, int quad, int hold, int after) {
    Game* g = s->game;
    const FoldoutMenu& m = g->m_interface.m_foldoutMenu;
    const QuadVertex* v = &m.m_vertices[quad * 4];
    double x = static_cast<double>(m.m_offset.x) + (static_cast<double>(v[0].position.x) + v[2].position.x) / 2.0;
    double y = static_cast<double>(m.m_offset.y) + (static_cast<double>(v[0].position.y) + v[2].position.y) / 2.0;
    g->TouchStart(static_cast<float>(x), static_cast<float>(y / 1.5));
    dr_Session_Frames(s, hold);
    g->TouchEnd();
    dr_Session_Frames(s, after);
}

// The session's Game (for the field accessors of drtest_api.cpp, e.g. dr_Game_GetField).
DRTEST_API Game* dr_Session_Game(Session* s) { return s->game; }

DRTEST_API int dr_Session_Turn(Session* s) { return s->game->m_turn; }
DRTEST_API int dr_Session_Health(Session* s) { return s->game->m_health; }
DRTEST_API int dr_Session_GameOver(Session* s) { return s->game->m_gameOverScreen.m_active ? 1 : 0; }
// -1 when no fold-out menu is open, else the type it was opened with (0 level-up, 1 shop, 2 upgrade)
DRTEST_API int dr_Session_Menu(Session* s) {
    const FoldoutMenu& m = s->game->m_interface.m_foldoutMenu;
    return m.m_active ? m.m_type : -1;
}
DRTEST_API int dr_Session_TileType(Session* s, int row, int col) { return s->game->m_board[row][col].m_type; }

// Game::SerialiseState; copies up to cap bytes, returns the size.
DRTEST_API int dr_Session_SaveState(Session* s, uint8_t* out, int cap) {
    Game* g = s->game;
    g->SerialiseState();
    const GameState& st = g->m_saveState;
    int n = st.GetStateSize();
    for (int i = 0; i < n && i < cap; ++i) out[i] = st.GetChar(i);
    return n;
}

// Unsupported hook calls so far (one per line); returns the length.
DRTEST_API int dr_Session_Events(Session* s, char* out, int cap) {
    int n = static_cast<int>(s->events.size());
    if (out && cap > 0) {
        int k = n < cap - 1 ? n : cap - 1;
        std::memcpy(out, s->events.data(), static_cast<size_t>(k));
        out[k] = '\0';
    }
    return n;
}
