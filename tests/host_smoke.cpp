// Smoke test of the shared host layer (platform/common): boots the game the way a host does, with
// the repo's real assets and a fake platform (decodes but does not upload or play), in a scratch
// save folder; runs frames and checks what a host relies on: all 6 textures and 70 sound files
// load, the fonts are read, resizing moves the viewport, Back on the main menu asks to quit, a
// changed setting is saved and read back by the next start.
//
// Usage: host_smoke <assets dir> <scratch dir>     Exit code 0 = all checks passed.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string>

#include "game.h"
#include "skill.h"
#include "gl_record.h"
#include "host.h"
#include "media.h"
#include "progress_code.h"
#include "util.h"

namespace {

int g_failures = 0;

void Check(bool ok, const char* what) {
    std::printf("%-48s %s\n", what, ok ? "OK" : "FAILED");
    if (!ok) g_failures++;
}

class FakePlatform : public host::Platform {
public:
    int textures = 0;
    int sounds = 0;
    int played = 0;

    bool LoadTexture(int glName, const std::string& path) override {
        host::Image image;
        if (glName < 1 || glName > 6 || !host::LoadImageRgba(path, image, true)) return false;
        if (image.width <= 0 || image.height <= 0) return false;
        textures++;
        return true;
    }
    int LoadSound(const std::string& path) override {
        host::Pcm pcm;
        if (!host::DecodeOgg(path, pcm) || pcm.samples.empty()) return -1;
        return sounds++;
    }
    void PlayEffect(int /*id*/) override { played++; }
    void SetVolume(float g) override { gain = g; }
    float gain = -1.0f;
    bool SetClipboardText(const std::string& text) override {
        clipboard = text;
        return true;
    }
    std::string GetClipboardText() override { return clipboard; }
    std::string clipboard;
};

// A fake clock: every read is 1/60 s later, so the frames run at game speed however fast the
// test runs them (fades and the intro finish).
int32_t g_micros = 0;
int32_t FakeClock() { return g_micros += 16667; }

// The score dates: a settable "now" (Unix seconds).
uint32_t g_now = 0;
uint32_t FakeNow() { return g_now; }

std::string ShortDate(uint32_t t) {   // as the score page shows it
    const std::time_t when = static_cast<std::time_t>(t);
    char text[16] = "";
    if (const std::tm* lt = std::localtime(&when)) std::strftime(text, sizeof text, "%d.%m.%y", lt);
    return text;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: host_smoke <assets dir> <scratch dir>\n");
        return 2;
    }
    namespace fs = std::filesystem;
    const fs::path scratch = fs::u8path(argv[2]);
    std::error_code ec;
    fs::remove_all(scratch, ec);
    fs::create_directories(scratch);

    SetClockSource(FakeClock);
    FakePlatform platform;
    host::Config config;
    config.assetDir = argv[1];
    config.saveDir = scratch.u8string();
    {
        host::Host h(platform, config);
        Check(h.Init(640, 960), "Init");
        Check(platform.textures == 6, "6 textures decoded");
        Check(platform.sounds == 70, "70 sound files decoded");
        Game* g = h.GetGame();
        Check(g != nullptr, "game created");
        if (!g) return 1;
        Check(g->m_fonts[0].GetLineHeight() > 0 && g->m_fonts[1].GetLineHeight() > 0 &&
                  g->m_fonts[2].GetLineHeight() > 0,
              "3 fonts read");
        // the tall canvas (9:16): a 640 x 960 view is wider, so bars left and right
        Check(g->m_tallCanvas && g->m_viewportWidth == 540 && g->m_viewportHeight == 960 && g->m_viewportX == 50,
              "9:16 viewport 540x960 in a 640x960 view");

        for (int i = 0; i < 300; i++) {
            h.Frame();
            GlTraceReset();
        }
        Check(g->m_mainMenu.m_active, "main menu showing after 300 frames");

        h.Resize(1000, 900);   // wider than 9:16: bars left and right
        Check(g->m_viewportHeight == 900 && g->m_viewportWidth == 506 && g->m_viewportX == 247,
              "resize recomputes the viewport");
        Check(!g->IsBoardScreen() && g->m_screenOffsetY > 0.138f && g->m_screenOffsetY < 0.14f,
              "the title screen is centred on the tall canvas");

        Check(!h.Back(), "Back on the main menu asks to quit");

        // The settings screen (port-only), over the title screen. Its rows (screen units, 1 wide):
        // panel top 0.015625, title bar 0.075, then rows of 0.1 every 0.125 after a 0.025 gap.
        auto frames = [&h](int n) {
            for (int i = 0; i < n; i++) {
                h.Frame();
                GlTraceReset();
            }
        };
        g->m_settingsScreen.Show();
        frames(30);   // faded in
        Check(g->m_settingsScreen.m_active, "the settings screen opens");
        const float rowY = 0.015625f + 0.075f + 0.025f + 2 * 0.125f + 0.05f;   // row 2: Clock
        const float vx = static_cast<float>(g->m_viewportX), vw = static_cast<float>(g->m_viewportWidth);
        // view pixels of a point in screen units (1 wide); the centred screens are m_screenOffsetY down
        auto tap = [&](float gx, float gy) {   // (the viewport as it is now: Info panel None changes it)
            const float x0 = static_cast<float>(g->m_viewportX), w = static_cast<float>(g->m_viewportWidth);
            const float y = (gy + g->m_screenOffsetY) * w + static_cast<float>(g->m_viewportY);
            h.TouchStart(x0 + gx * w, y);
            frames(2);
            h.TouchEnd();
            frames(2);
        };
        Check(!g->m_clockEnabled, "the clock is off at first");
        tap(0.5f, rowY);
        Check(g->m_clockEnabled, "a tap on the Clock row turns the clock on");

        // Row 0: the volume slider. Track from 40% of the row (x 0.046875, w 0.90625) to the value text.
        const float trackX = 0.046875f + 0.90625f * 0.4f;
        const float trackW = (0.046875f + 0.90625f - 0.03125f - 0.14f) - trackX;
        const float volumeY = 0.015625f + 0.075f + 0.025f + 0.05f;
        Check(platform.gain == 1.0f && g->m_soundVolume == 100, "full volume at first");
        tap(trackX + 0.5f * trackW, volumeY);
        Check(g->m_soundVolume == 50 && g->m_soundEnabled, "the volume slider sets 50%");
        Check(platform.gain > 0.249f && platform.gain < 0.251f, "50% is a quarter of the amplitude");
        tap(trackX - 0.02f, volumeY);
        Check(g->m_soundVolume == 0 && !g->m_soundEnabled, "the slider at 0 turns the sound off");
        Check(h.Back(), "Back with the settings open does not quit");
        frames(30);   // faded out
        Check(!g->m_settingsScreen.m_active && g->m_mainMenu.m_active, "Back closes the settings, the title screen stays");

        // Debug mode: three taps on the build info (bottom right of the title screen).
        const Label& version = g->m_mainMenu.m_versionLabel;
        const float infoX = version.m_position.x + version.m_width * version.m_scale.x * 0.5f;
        const float infoY = version.m_position.y - version.m_scale.y * 0.3f;
        Check(!g->m_debugMode, "debug mode is off at first");
        for (int i = 0; i < 3; i++) tap(infoX, infoY);
        Check(g->m_debugMode && g->m_toast.m_active, "three taps on the build info turn debug mode on, with a toast");
        frames(200);   // the toast goes
        Check(!g->m_toast.m_active, "the toast fades out");

        // The debug action (Ctrl+S) adds random new skills, on the board only, up to the 4 slots.
        h.DebugAddSkill();
        Check(g->m_skillCount == 0, "no skill added on the title screen");
        g->m_mainMenu.SetActive(false);   // as if a game were running
        for (int i = 0; i < 4; i++) h.DebugAddSkill();
        bool distinct = g->m_skillCount == 4;
        for (int i = 0; i < g->m_skillCount; i++) {
            for (int j = i + 1; j < g->m_skillCount; j++) distinct = distinct && g->m_skills[i]->m_type != g->m_skills[j]->m_type;
        }
        Check(distinct && g->m_toast.Text().rfind("Debug: added skill ", 0) == 0, "four debug skills, all different, each with a toast");
        h.DebugAddSkill();
        Check(g->m_skillCount == 4 && g->m_toast.Text() == "All 4 skill slots are full", "a fifth: no free slot, a toast instead");

        // The debug page's actions (debug mode is on): a special monster, and the pause menu's button.
        g->DebugSpawnSpecialEnemy();
        Check(g->GetSpecialEnemyCount() >= 1 && g->m_toast.Text().rfind("Debug: added ", 0) == 0,
              "Add special monster puts a special enemy on the board, with a toast");
        g->m_interface.m_gameMenu.Show();
        Check(g->m_interface.m_gameMenu.m_debugShown, "the pause menu shows Debug in debug mode");
        {
            const GameMenu& menu = g->m_interface.m_gameMenu;
            const float exitY = menu.m_vertices[15 * 4].position.y;
            const float debugY = menu.m_vertices[GameMenu::kDebugQuad * 4].position.y;
            Check(debugY < exitY && exitY - debugY > 0.1f && exitY - debugY < 0.11f,
                  "Debug is above Exit (main menu), a button row apart");
        }
        g->m_interface.m_gameMenu.m_active = false;
        g->m_flareCount = 0;   // (the spawn's flare; no frames run here)

        // The info panel (tall canvas, board screen): the status, then hover over a skill and a tile.
        {   // (no frames here: with the title screen off they would run the game's turn on this board)
            Check(g->IsBoardScreen(), "with the title screen off the board is the screen");
            std::vector<InfoPanel::Line> lines = g->m_infoPanel.BuildLines();
            Check(!lines.empty() && lines[0].text.rfind("Enemies on the board: ", 0) == 0, "the panel shows the status");
            g->m_hoverActive = true;
            g->m_hoverX = g->m_interface.m_skillButtons[0].m_position.x;
            g->m_hoverY = g->m_interface.m_skillButtons[0].m_position.y;
            lines = g->m_infoPanel.BuildLines();
            Check(!lines.empty() && lines[0].text.rfind(Skill::GetSkillName(g->m_skills[0]->m_type), 0) == 0,
                  "hovering a skill button shows the skill");
            g->m_hoverX = g->m_gridPositions[2][3].x;
            g->m_hoverY = g->m_gridPositions[2][3].y;
            lines = g->m_infoPanel.BuildLines();
            Check(!lines.empty() && lines.size() <= 5, "hovering a tile describes it (at most 5 lines)");
            g->m_hoverX = -0.05f;   // the frame left of the board (GetTileAtPosition rounds it to column 0)
            lines = g->m_infoPanel.BuildLines();
            Check(!lines.empty() && lines[0].text.rfind("Enemies on the board: ", 0) == 0,
                  "hovering the frame left of the board shows the status, not the column-0 tile");
            g->m_hoverX = g->m_gridPositions[2][3].x;

            // Every special enemy: the stats on the name line, the description at the largest text
            // size that fits it (counted per size), and how many are still cut at the smallest.
            {
                Tile& t = g->m_board[2][3];
                const Tile saved = t;
                t.m_type = TILE_SKULL;
                t.m_frozen = t.m_burning = t.m_spawnedThisTurn = false;
                t.m_countdown = 0;
                int bySize[3] = {0, 0, 0}, cut = 0;
                bool statsOnName = true;
                for (int e = 1; e < ENEMY_TYPE_COUNT; ++e) {
                    t.m_enemyType = static_cast<EnemyType>(e);
                    lines = g->m_infoPanel.BuildLines();
                    statsOnName = statsOnName && !lines.empty() && lines[0].text.rfind(Tile::GetEnemyName(t.m_enemyType), 0) == 0 &&
                                  lines[0].text.find("Attack ") != std::string::npos;
                    const float s = g->m_infoPanel.TextScale();
                    bySize[s > 0.075f ? 0 : s > 0.067f ? 1 : 2] += 1;
                    if (!lines.empty() && lines.back().text.size() >= 3 &&
                        lines.back().text.compare(lines.back().text.size() - 3, 3, "...") == 0) {
                        ++cut;
                        std::printf("  still cut: %s\n", Tile::GetEnemyName(t.m_enemyType));
                    }
                }
                std::printf("  special enemies by text size: normal %d, smaller %d, smallest %d; cut %d\n", bySize[0], bySize[1],
                            bySize[2], cut);
                t.m_enemyType = ENEMY_NORMAL;
                lines = g->m_infoPanel.BuildLines();
                Check(!lines.empty() && lines[0].text.rfind("Enemy   Attack ", 0) == 0,
                      "a normal enemy is called Enemy (not the table's None)");
                t = saved;
                Check(statsOnName, "an enemy's stats share the line with its name");
                Check(cut == 0, "every special enemy's description fits (shrinking the text if needed)");
            }
            g->PointerLeave();

            // Touches (set directly, no frames): a skill press shows the skill, kept after a release
            // off the button; a tap on a tile shows the tile, kept; a new turn drops it.
            const std::string skillName = Skill::GetSkillName(g->m_skills[1]->m_type);
            g->m_touchState = TOUCHING;
            g->m_touchX = g->m_interface.m_skillButtons[1].m_position.x;
            g->m_touchY = g->m_interface.m_skillButtons[1].m_position.y;
            lines = g->m_infoPanel.BuildLines();
            Check(!lines.empty() && lines[0].text.rfind(skillName, 0) == 0, "pressing a skill shows it");
            g->m_touchX = 0.5f;   // slide off the button
            g->m_touchY = 0.7f;
            g->m_infoPanel.BuildLines();
            g->m_touchState = NOT_TOUCHING;
            lines = g->m_infoPanel.BuildLines();
            Check(!lines.empty() && lines[0].text.rfind(skillName, 0) == 0, "released off the button: the skill stays");
            g->m_touchState = TOUCHING;
            g->m_touchX = g->m_gridPositions[4][1].x;
            g->m_touchY = g->m_gridPositions[4][1].y;
            const std::vector<InfoPanel::Line> held = g->m_infoPanel.BuildLines();
            g->m_touchState = NOT_TOUCHING;
            lines = g->m_infoPanel.BuildLines();
            Check(!held.empty() && held[0].text.rfind("Enemies on the board", 0) != 0 && lines == held,
                  "a tap on a tile shows it, and it stays");
            g->m_hoverActive = true;   // the mouse over the panel (above the board: y -0.278 .. 0)
            g->m_hoverX = 0.5f;
            g->m_hoverY = -0.1f;
            lines = g->m_infoPanel.BuildLines();
            Check(!lines.empty() && lines[0].text.rfind("Enemies on the board: ", 0) == 0,
                  "the mouse over the panel shows the status, not the kept info");
            g->PointerLeave();
            lines = g->m_infoPanel.BuildLines();
            Check(lines == held, "leaving the window shows the kept info again");
            // a menu over the board: the status, even with the mouse over a tile; the kept info goes
            g->m_interface.m_statsMenu.m_active = true;
            g->m_hoverActive = true;
            g->m_hoverX = g->m_gridPositions[4][1].x;
            g->m_hoverY = g->m_gridPositions[4][1].y;
            lines = g->m_infoPanel.BuildLines();
            Check(!lines.empty() && lines[0].text.rfind("Enemies on the board: ", 0) == 0,
                  "with a menu open the panel shows the status");
            g->m_interface.m_statsMenu.m_active = false;
            g->PointerLeave();
            lines = g->m_infoPanel.BuildLines();
            Check(!lines.empty() && lines[0].text.rfind("Enemies on the board: ", 0) == 0,
                  "after the menu the kept info is gone");
            g->m_turn += 1;
            lines = g->m_infoPanel.BuildLines();
            Check(!lines.empty() && lines[0].text.rfind("Enemies on the board: ", 0) == 0, "a new turn shows the status again");
            g->m_turn -= 1;

            // The status's turn line: the turn, the next special enemy, the next trophy (port).
            {
                const Difficulty savedDiff = g->m_difficulty;
                const bool savedChallenge = g->m_challenge;
                const GameMode savedMode = g->m_mode;
                const int savedTurn = g->m_turn, savedPromo = g->m_timeToPromotion;
                const int savedTimer = g->m_timeToTrophy, savedSpawned = g->m_trophiesSpawned, savedGrind = g->m_grind;
                auto status = [&] {   // the status lines joined, '|' between lines
                    std::string s;
                    for (const InfoPanel::Line& l : g->m_infoPanel.BuildLines()) s += l.text + "|";
                    return s;
                };
                auto has = [&](const std::string& s, const char* part) { return s.find(part) != std::string::npos; };
                g->m_challenge = false;
                g->m_difficulty = DIFF_NORMAL;
                g->m_grind = 0;
                g->m_trophiesSpawned = 0;
                g->m_turn = 36;
                g->m_timeToPromotion = 3;
                g->m_timeToTrophy = 7;
                std::string s = status();
                Check(has(s, "Turn 37") && has(s, "Special in 4 turns") && has(s, "Trophy in 7 kills"),
                      "status: turn 37, special in 4 turns, trophy in 7 kills");
                g->m_timeToPromotion = 0;
                g->m_timeToTrophy = 0;
                s = status();
                Check(has(s, "Special: next skull") && has(s, "Trophy in 1 kill|"), "status: special with the next skull, trophy in 1 kill");
                g->m_grind = 2;
                g->m_timeToTrophy = 25;
                Check(has(status(), "Trophy in 8 kills"), "status: Faster grind cuts the trophy countdown to 8");
                g->m_grind = 0;
                g->m_trophiesSpawned = 3;
                Check(has(status(), "No more trophies this game"), "status: no trophy after the per-game cap");
                g->m_difficulty = DIFF_EASY;
                s = status();
                Check(!has(s, "rophies") && !has(s, "Trophy"), "status: nothing about trophies on Easy");
                g->m_difficulty = DIFF_NORMAL;
                g->m_trophiesSpawned = 0;
                g->m_timeToTrophy = 5;
                g->m_challenge = true;
                g->m_mode = MODE_SPRINT;
                g->m_timeToPromotion = 200;
                g->m_turn = 5;   // specials on turns 4, 14, 24 ... (m_turn after the refill)
                s = status();
                Check(has(s, "Turn 6") && has(s, "Special in 9 turns") && has(s, "Trophy in ~10 kills"),
                      "status (Sprint): special in 9 turns, trophy in about twice the kills");
                g->m_turn = 13;
                Check(has(status(), "Special: next turn"), "status (Sprint): special next turn");
                g->m_difficulty = savedDiff;
                g->m_challenge = savedChallenge;
                g->m_mode = savedMode;
                g->m_turn = savedTurn;
                g->m_timeToPromotion = savedPromo;
                g->m_timeToTrophy = savedTimer;
                g->m_trophiesSpawned = savedSpawned;
                g->m_grind = savedGrind;
            }
            g->m_infoPanel.BuildLines();
            g->m_infoPanel.Render();   // lays out and draws (into the recorded GL calls)
            GlTraceReset();
        }

        // Grind (port): the trophy countdown's range and the per-game cap (killing one tile directly).
        {
            Tile& t = g->m_board[1][1];
            const Tile savedTile = t;
            const Difficulty savedDiff = g->m_difficulty;
            const bool savedChallenge = g->m_challenge;
            const int savedTimer = g->m_timeToTrophy, savedSpawned = g->m_trophiesSpawned, savedGrind = g->m_grind;
            const int savedQueue = g->m_achievementCount;
            g->m_difficulty = DIFF_NORMAL;
            g->m_challenge = false;
            auto kill = [&] {   // a special enemy killed: is it a trophy now?
                t.m_type = TILE_SKULL;
                t.m_enemyType = ENEMY_NORMAL;
                t.m_specialType = SPECIAL_TILE_NONE;
                g->KillSpecialEnemyTile(&t);
                return t.m_specialType == SPECIAL_TILE_TROPHY;
            };
            g->m_grind = 0;
            g->m_trophiesSpawned = 0;
            g->m_timeToTrophy = 2;
            const bool first = kill();
            const bool second = kill();
            Check(!first && second && g->m_timeToTrophy >= 20 && g->m_timeToTrophy <= 30,
                  "Original: the countdown reaches 0, a trophy, the next in 20..30 kills");
            g->m_trophiesSpawned = 3;
            g->m_timeToTrophy = 1;
            Check(!kill() && g->m_timeToTrophy == 0, "Original: no 4th trophy in a game");
            g->m_grind = 1;
            g->m_trophiesSpawned = 3;
            g->m_timeToTrophy = 25;
            Check(!kill() && g->m_timeToTrophy == 14, "Fast: a longer countdown is cut to 15 (then counts)");
            g->m_timeToTrophy = 1;
            Check(kill() && g->m_timeToTrophy >= 10 && g->m_timeToTrophy <= 15, "Fast: a 4th trophy, the next in 10..15");
            g->m_trophiesSpawned = 5;
            g->m_timeToTrophy = 1;
            Check(!kill(), "Fast: no 6th trophy in a game");
            g->m_grind = 2;
            g->m_trophiesSpawned = 5;
            g->m_timeToTrophy = 30;
            const bool cut = !kill() && g->m_timeToTrophy == 7;
            g->m_timeToTrophy = 1;
            Check(cut && kill() && g->m_timeToTrophy >= 5 && g->m_timeToTrophy <= 8,
                  "Faster: countdown cut to 8, a 6th trophy, the next in 5..8");
            t = savedTile;
            g->m_difficulty = savedDiff;
            g->m_challenge = savedChallenge;
            g->m_timeToTrophy = savedTimer;
            g->m_trophiesSpawned = savedSpawned;
            g->m_grind = savedGrind;
            g->m_timeToTrophyChanged = false;
            g->m_achievementCount = savedQueue;   // (the kills' achievements: not for the checks below)
        }

        // Score dates (port): two Normal scores at two times; the higher one ranks first, each keeps its date.
        g->m_mainMenu.SetActive(true);
        SetTimeSource(FakeNow);
        g_now = 1700000000;   // 14.11.23
        g->AddScore(10, 1, 2, 3, 100, static_cast<Difficulty>(1));
        g_now = 1760000000;   // 09.10.25
        g->AddScore(20, 1, 2, 3, 200, static_cast<Difficulty>(1));
        SetTimeSource(nullptr);
        const ScoreScreen::Entry* normal = g->m_scoreScreen.m_entries[1];
        Check(normal[0].score == 200 && normal[0].time == 1760000000 && normal[1].score == 100 && normal[1].time == 1700000000,
              "scores keep their dates when the table reorders");
        frames(2);   // the host writes the score file
        g->ShowScores(1, false);
        frames(40);
        Check(g->m_scoreScreen.m_active && g->m_scoreScreen.m_pages[1]->m_dates[0].m_text == ShortDate(1760000000),
              "the score page shows the date (dd.mm.yy)");
        h.Back();
        frames(40);

        // Achievements (port): amounts add up to 100 (then unlocked, dated, a toast), the host's queue
        // drain, the debug action, the "ach" file, the list screen.
        {
            frames(200);   // no toast showing (the next one would wait for it)
            SetTimeSource(FakeNow);
            g_now = 1763020800;   // 13.11.25
            g->AddAchievementProgress(ACH_BLOODLUST, 60.0f);
            Check(!g->IsAchievementUnlocked(ACH_BLOODLUST) && g->m_achievementRecords[ACH_BLOODLUST].progress == 60.0f,
                  "an achievement's progress adds up");
            g->AddAchievementProgress(ACH_BLOODLUST, 50.0f);
            Check(g->IsAchievementUnlocked(ACH_BLOODLUST) && g->m_achievementRecords[ACH_BLOODLUST].progress == 100.0f &&
                      g->m_achievementRecords[ACH_BLOODLUST].time == 1763020800 &&
                      g->m_toast.Text() == "Achievement unlocked: Bloodlust",
                  "at 100 it unlocks, capped, dated, with a toast");
            g_now = 1763030000;
            g->AddAchievementProgress(ACH_BLOODLUST, 5.0f);
            Check(g->m_achievementRecords[ACH_BLOODLUST].time == 1763020800, "an unlocked achievement takes no more");
            const Difficulty difficulty = g->m_difficulty;
            g->m_difficulty = DIFF_NORMAL;   // (Easy queues only Mission Accomplished)
            g->QueueAchievement(ACH_JACKPOT, 100.0f);
            g->QueueAchievement(ACH_PACK_RAT, 1.0f);
            g->ProcessAchievementQueue();
            g->m_difficulty = difficulty;
            Check(g->IsAchievementUnlocked(ACH_JACKPOT) && g->m_achievementRecords[ACH_PACK_RAT].progress == 1.0f,
                  "the queue is drained into the records");
            g->DebugAddAchievement();
            Check(g->IsAchievementUnlocked(ACH_RANGER) && g->UnlockedAchievementCount() == 3,
                  "the debug action unlocks the first locked achievement");
            SetTimeSource(nullptr);
            frames(2);   // the host writes ach
            Check(fs::exists(scratch / "ach") && !g->m_achievementsChanged, "the achievements are saved (ach)");
            std::vector<uint8_t> saved = g->SaveAchievements();
            Game::AchievementRecord records[kAchievementCount];
            Check(Game::ParseAchievementRecords(saved.data() + 4, saved.size() - 4, records) &&
                      records[ACH_BLOODLUST].time == 1763020800,
                  "the ach bytes parse back");
            saved[4 + 1 + (ACH_JACKPOT - 1) * 5] = 99;   // unlocked date but not 100%
            Check(!g->LoadAchievements(saved) && g->IsAchievementUnlocked(ACH_JACKPOT), "a damaged ach file is not loaded");

            g->m_achievementsScreen.Show();
            frames(30);
            Check(g->m_achievementsScreen.m_active && g->m_achievementsScreen.m_maxScroll > 0.0f &&
                      g->m_achievementsScreen.m_scroll == 0.0f,
                  "the achievements screen opens at the top of a scrolling list");
            // unlocked first, newest first (Ranger and Jackpot at the same time: in the list's order),
            // then the locked ones in the list's order
            const AchievementsScreen& list = g->m_achievementsScreen;
            Check(list.AchievementAt(0) == ACH_RANGER && list.AchievementAt(1) == ACH_JACKPOT &&
                      list.AchievementAt(2) == ACH_BLOODLUST && list.AchievementAt(3) == ACH_RAIDER &&
                      list.AchievementAt(4) == ACH_PRIEST,
                  "the list: unlocked ones newest first, then the locked ones in order");
            h.MouseWheel(3.0f);
            Check(g->m_achievementsScreen.m_scroll > 0.5f && g->m_achievementsScreen.m_scroll < 0.6f,
                  "the mouse wheel scrolls it (a row per notch)");
            h.MouseWheel(-1000.0f);
            Check(g->m_achievementsScreen.m_scroll == 0.0f, "and stops at the top");
            // drags (screen units): the bar's thumb (x 0.947, at the top: y 0.10..0.20) follows the
            // pointer down, so the list scrolls down; a drag on the list moves it with the finger
            auto drag = [&](float gx, float y0, float y1) {
                auto py = [&](float gy) { return (gy + g->m_screenOffsetY) * vw + static_cast<float>(g->m_viewportY); };
                h.TouchStart(vx + gx * vw, py(y0));
                frames(2);
                h.TouchDrag(vx + gx * vw, py(y1));
                frames(2);
                h.TouchEnd();
                frames(2);
            };
            drag(0.947f, 0.15f, 0.45f);
            const float barScroll = g->m_achievementsScreen.m_scroll;
            Check(barScroll > 2.0f, "dragging the scroll bar down scrolls the list down");
            drag(0.5f, 0.6f, 0.8f);
            Check(g->m_achievementsScreen.m_scroll < barScroll - 0.15f, "dragging the list down scrolls it up");
            // a finger: a swipe on the bar's track (not the thumb) moves the list; it flings on and
            // stops; the mouse jumps the thumb instead and does not fling
            auto dragBy = [&](bool mouse, float gx, float y0, float y1) {
                auto py = [&](float gy) { return (gy + g->m_screenOffsetY) * vw + static_cast<float>(g->m_viewportY); };
                h.TouchStart(vx + gx * vw, py(y0), mouse);
                frames(2);
                h.TouchDrag(vx + gx * vw, py(y1));
                frames(2);
                h.TouchEnd();
            };
            auto settle = [&]() { frames(400); };
            h.MouseWheel(-1000.0f);
            frames(2);
            dragBy(false, 0.947f, 1.1f, 0.9f);
            frames(2);
            const float swiped = g->m_achievementsScreen.m_scroll;
            Check(swiped > 0.15f && swiped < 0.6f, "a finger on the bar's track scrolls the list (not the thumb)");
            frames(30);
            Check(g->m_achievementsScreen.m_scroll > swiped + 0.01f, "the list flings on after the finger lifts");
            settle();
            const float settled = g->m_achievementsScreen.m_scroll;
            frames(10);
            Check(g->m_achievementsScreen.m_scroll == settled, "the fling slows down and stops");
            h.MouseWheel(-1000.0f);
            dragBy(true, 0.5f, 0.9f, 0.7f);
            frames(2);
            const float mouseDragged = g->m_achievementsScreen.m_scroll;
            frames(30);
            Check(mouseDragged > 0.15f && g->m_achievementsScreen.m_scroll == mouseDragged, "a mouse drag does not fling");
            h.MouseWheel(-1000.0f);
            h.TouchStart(vx + 0.947f * vw, (1.0f + g->m_screenOffsetY) * vw + static_cast<float>(g->m_viewportY), true);
            frames(2);
            h.TouchEnd();
            frames(2);
            Check(g->m_achievementsScreen.m_scroll > 2.0f, "a mouse press on the bar's track jumps the thumb there");
            Check(!g->m_mainMenu.m_active || !g->m_settingsScreen.m_active, "(nothing else opened)");
            h.Back();
            frames(30);
            Check(!g->m_achievementsScreen.m_active, "Back closes the achievements screen");
        }

        // Progress codes (port): the codec, then Export / Import through the settings and the clipboard.
        {
            const std::string code = progress::Export(*g);
            progress::Data data;
            Check(code.rfind("DR2-", 0) == 0 && progress::Parse(code, data) == progress::Error::None &&
                      data.classes[1].level == g->m_classes[1].m_level &&
                      data.achievementsUnlocked == g->UnlockedAchievementCount(),
                  "a progress code parses back (with the achievements)");
            Check(progress::Parse("DR1-" + code.substr(4), data) == progress::Error::BadVersion,
                  "an old (version 1) code is rejected");
            std::string wrapped = code;
            wrapped.insert(20, "\r\n  ");
            Check(progress::Parse(" " + wrapped + "\n", data) == progress::Error::None, "a code with whitespace parses");
            std::string damaged = code;
            damaged[30] = damaged[30] == 'A' ? 'B' : 'A';
            Check(progress::Parse(damaged, data) == progress::Error::BadChecksum, "a changed character fails the CRC");
            Check(progress::Parse(code.substr(0, code.size() - 4), data) == progress::Error::BadSize,
                  "a cut code is rejected");
            Check(progress::Parse("hello", data) == progress::Error::NotACode &&
                      progress::Parse(" \n", data) == progress::Error::Empty &&
                      progress::Parse("DR2-ab*d", data) == progress::Error::BadEncoding,
                  "no code, empty, bad characters are rejected");
            const uint8_t abc[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
            Check(progress::Crc32(abc, sizeof abc) == 0xcbf43926u, "CRC-32 check value");
        }
        // Rows over the title screen (debug mode is on, no mouse): Volume, Help pages, Clock,
        // Attack dimming, Grind (4), Info panel (5), Export progress (6), Import progress (7), Debug mode.
        auto rowAt = [](int row) { return 0.015625f + 0.075f + 0.025f + static_cast<float>(row) * 0.125f + 0.05f; };
        const int before = g->m_classes[3].m_level;
        g->m_classes[3].SetLevel(5);   // the progress to export
        g->m_classes[3].m_gamesPlayed = 7;
        g->m_settingsScreen.Show();
        frames(30);
        Check(g->m_grind == 0, "Grind is Original at first");
        tap(0.5f, rowAt(4));
        Check(g->m_grind == 1, "a tap on Grind makes it Fast");
        tap(0.5f, rowAt(4));
        tap(0.5f, rowAt(4));
        Check(g->m_grind == 0, "two more taps: Faster, then Original again");
        tap(0.5f, rowAt(4));
        tap(0.5f, rowAt(4));
        Check(g->m_grind == 2, "Grind set to Faster (saved)");
        Check(g->m_panelMode == Game::PANEL_TOP && g->m_tallCanvas && !g->m_panelAtBottom,
              "the info panel is above the board at first");
        tap(0.5f, rowAt(5));
        Check(g->m_panelMode == Game::PANEL_BOTTOM && g->m_panelAtBottom, "a tap on Info panel moves it below the board");
        tap(0.5f, rowAt(5));
        frames(2);
        Check(g->m_panelMode == Game::PANEL_NONE && !g->m_tallCanvas && g->m_viewportWidth * 3 == g->m_viewportHeight * 2 &&
                  g->m_screenOffsetY == 0.0f,
              "the next tap: None, the original 2:3 screen (no Horizontal without a desktop)");
        tap(0.5f, rowAt(5));
        frames(2);
        Check(g->m_panelMode == Game::PANEL_TOP && g->m_tallCanvas &&
                  std::abs(g->m_viewportWidth * 16 - g->m_viewportHeight * 9) < 16,
              "the next tap: Top again, the 9:16 canvas");
        tap(0.5f, rowAt(5));
        Check(g->m_panelMode == Game::PANEL_BOTTOM, "Info panel set to Bottom (saved)");
        tap(0.5f, rowAt(6));
        Check(platform.clipboard.rfind("DR2-", 0) == 0 && g->m_toast.Text() == "Progress copied to the clipboard",
              "Export progress copies a code, with a toast");
        g->m_classes[3].SetLevel(before);   // as if on another device
        g->m_classes[3].m_gamesPlayed = 0;
        const Game::AchievementRecord nomNom = g->m_achievementRecords[ACH_NOM_NOM];
        g->m_achievementRecords[ACH_JACKPOT] = Game::AchievementRecord();   // not in the code: the import brings it back
        tap(0.5f, rowAt(7));
        Check(g->m_settingsScreen.m_dialog, "Import progress opens the dialog");
        // the dialog's Import button: panel at y 0.1, 14 lines of 0.062 (see OpenDialog)
        const float panelH = 0.075f + 0.04f + 14.0f * 0.062f + 0.03f + 0.05f + 0.1f + 0.04f;
        tap(0.015625f + 0.0625f + 0.2f, 0.1f + panelH - 0.04f - 0.05f);
        Check(!g->m_settingsScreen.m_dialog && g->m_classes[3].m_level == 5 && g->m_classes[3].m_gamesPlayed == 7 &&
                  g->m_toast.Text() == "Progress imported" && g->IsAchievementUnlocked(ACH_JACKPOT) &&
                  g->m_achievementRecords[ACH_NOM_NOM].progress == nomNom.progress,
              "the dialog's Import applies the code (classes and achievements)");
        platform.clipboard = "not a code";
        tap(0.5f, rowAt(7));
        Check(!g->m_settingsScreen.m_dialog && g->m_toast.Text() == progress::ErrorText(progress::Error::NotACode),
              "importing something else shows an error toast");
        h.Back();
        frames(30);
    }
    Check(fs::exists(scratch / "settings.ini"), "a changed setting writes settings.ini");
    {
        host::Host h(platform, config);
        Check(h.Init(640, 960) && h.GetGame()->m_clockEnabled, "the setting is read back by the next start");
        Check(!h.GetGame()->m_soundEnabled && h.GetGame()->m_soundVolume == 0 && platform.gain == 0.0f,
              "the sound stays off after a restart");
        Check(h.GetGame()->m_debugMode, "debug mode stays on after a restart");
        Check(h.GetGame()->m_classes[3].m_level == 5 && h.GetGame()->m_classes[3].m_gamesPlayed == 7,
              "imported progress is saved (cla) and read back");
        Check(h.GetGame()->m_panelMode == Game::PANEL_BOTTOM && h.GetGame()->m_panelAtBottom,
              "Info panel stays Bottom after a restart");
        Check(h.GetGame()->m_grind == 2, "Grind stays Faster after a restart");
        Check(h.GetGame()->IsAchievementUnlocked(ACH_BLOODLUST) &&
                  h.GetGame()->m_achievementRecords[ACH_BLOODLUST].time == 1763020800 &&
                  h.GetGame()->IsAchievementUnlocked(ACH_JACKPOT) && h.GetGame()->UnlockedAchievementCount() == 3,
              "the achievements are read back (ach) by the next start");
        const ScoreScreen::Entry* normal = h.GetGame()->m_scoreScreen.m_entries[1];
        Check(h.GetGame()->m_scoreScreen.m_counts[1] == 2 && normal[0].time == 1760000000 && normal[1].time == 1700000000,
              "the score dates are read back from the file");
        h.Frame();   // the clock string path
        GlTraceReset();

        // Reset progress (settings, title screen): No keeps everything; Yes wipes classes, scores,
        // achievements, the saved game; the settings stay. Rows (no mouse, debug mode on): ... Import
        // progress (7), Reset progress (8), Debug mode (9).
        Game* g = h.GetGame();
        auto frames = [&h](int n) {
            for (int i = 0; i < n; i++) {
                h.Frame();
                GlTraceReset();
            }
        };
        frames(300);
        const float vx = static_cast<float>(g->m_viewportX), vw = static_cast<float>(g->m_viewportWidth);
        auto tap = [&](float gx, float gy) {
            h.TouchStart(vx + gx * vw, (gy + g->m_screenOffsetY) * vw + static_cast<float>(g->m_viewportY));
            frames(2);
            h.TouchEnd();
            frames(2);
        };
        const float resetRow = 0.015625f + 0.075f + 0.025f + 8.0f * 0.125f + 0.05f;
        // the dialog: panel at y 0.1, 4 lines of 0.062; Yes left, No right (see OpenResetDialog)
        const float dialogH = 0.075f + 0.04f + 4.0f * 0.062f + 0.05f + 0.1f + 0.04f;
        const float buttonY = 0.1f + dialogH - 0.04f - 0.05f;
        g->m_settingsScreen.Show();
        frames(30);
        tap(0.5f, resetRow);
        Check(g->m_settingsScreen.m_dialog, "Reset progress asks first");
        tap(0.015625f + 0.96875f - 0.0625f - 0.2f, buttonY);   // No
        Check(!g->m_settingsScreen.m_dialog && g->m_classes[3].m_level == 5 && g->UnlockedAchievementCount() == 3 &&
                  g->m_scoreScreen.m_counts[1] == 2,
              "No keeps the progress");
        g->DebugUnlockAll();   // the debug page's Unlock all (debug mode is on); the reset below wipes it
        bool maxed = g->m_classes[0].m_level == 1;
        for (int c = 1; c < 10; ++c) maxed = maxed && g->m_classes[c].m_level == 10;
        bool skills = true;
        for (int s = 31; s < SKILL_COUNT; ++s) skills = skills && g->IsSkillUnlocked(static_cast<SkillType>(s));
        Check(maxed && skills && g->IsRaceUnlocked(RACE_ELF) && g->m_classes[3].IsPerkUnlocked(3) &&
                  g->m_classState.HasStateChanged() && g->m_toast.Text() == "Debug: all classes at level 10",
              "Unlock all: every class at level 10 (all races, skills, perk slots), saved");
        {   // the name screen's open but empty perk/flaw slots: the transparent corner, not the whole texture
            g->m_classes[3].m_perks[2] = PERK_NONE;
            g->m_classes[3].m_flaws[2] = FLAW_NONE;
            g->m_nameScreen.SelectClass(g->m_classes[3]);
            bool inside = true;
            for (int k : {4, 8}) {   // perk and flaw of slot 2
                for (const QuadVertex& v : g->m_nameScreen.m_propertyIcons[k].m_vertices) {
                    inside = inside && v.uv.x >= 0.0f && v.uv.x <= 1.0f && v.uv.y >= 0.0f && v.uv.y <= 1.0f;
                }
            }
            Check(inside, "empty perk/flaw slots use texture coordinates inside the texture");
        }
        tap(0.5f, resetRow);
        tap(0.015625f + 0.0625f + 0.2f, buttonY);   // Yes
        Check(!g->m_settingsScreen.m_dialog && g->m_classes[3].m_level == 0 && g->m_classes[0].m_level == 1 &&
                  g->m_classes[3].m_gamesPlayed == 0 && g->UnlockedAchievementCount() == 0 &&
                  g->m_scoreScreen.m_counts[1] == 0 && g->m_saveState.GetStateSize() == 0 &&
                  g->m_toast.Text() == "Progress reset",
              "Yes wipes classes, achievements, scores, the saved game");
        Check(g->m_clockEnabled && g->m_debugMode, "the settings stay");
        h.Back();
        frames(30);
    }
    {
        host::Host h(platform, config);
        Check(h.Init(640, 960), "Init (after the reset)");
        Game* g = h.GetGame();
        Check(g->m_classes[3].m_level == 0 && g->UnlockedAchievementCount() == 0 && g->m_scoreScreen.m_counts[1] == 0 &&
                  g->m_saveState.GetStateSize() == 0 && !fs::exists(scratch / "save"),
              "the reset is saved: the next start has no progress");
    }

    // The horizontal layout (desktop): a host of its own, so its game frames touch nothing above.
    config.desktop = true;
    {
        host::Host h(platform, config);
        Check(h.Init(640, 960), "Init (desktop)");
        Game* g = h.GetGame();
        auto frames = [&h](int n) {
            for (int i = 0; i < n; i++) {
                h.Frame();
                GlTraceReset();
            }
        };
        frames(300);
        Check(!g->IsWide() && g->m_desktop, "the horizontal layout is off at first, offered on a desktop");
        Check(!h.Fullscreen(), "Fullscreen is off at first");
        h.SetFullscreen(true);   // as Alt+Enter (saved by the next frame)
        frames(1);
        Check(h.Fullscreen() && !g->m_fullscreenChanged, "Alt+Enter: Fullscreen on, saved");

        // Vertical (Info panel Top): the title fading out over a new game draws the board where the
        // board screen has it (below the panel: its projection), not centred, so it doesn't jump.
        {
            g->SetPanelMode(Game::PANEL_TOP);
            frames(1);
            g->InitNewGame();
            g->m_nameScreen.m_active = false;
            g->m_mainMenu.FadeOut();   // as Continue
            frames(1);
            GlTraceReset();
            h.Frame();
            auto bits = [](float f) {
                uint32_t u;
                std::memcpy(&u, &f, sizeof u);
                char buf[16];
                std::snprintf(buf, sizeof buf, "%x", u);
                return std::string(buf);
            };
            const std::string boardOrtho = "glOrthof 0 3f800000 " + bits(Game::kTallCanvasHeight - Game::kPanelHeight) +
                                           " " + bits(-Game::kPanelHeight) + " ";
            Check(g->m_mainMenu.m_active && g->BoardUnderFade() && GlTrace().find(boardOrtho) != std::string::npos &&
                      g->m_screenOffsetY == Game::kPanelHeight * 0.5f,
                  "a fade over the board: the board below the panel already, the fading screen centred");
            GlTraceReset();
            g->m_mainMenu.m_fadeTime = 0.0f;   // back to the title screen, the fade undone (no game frames)
            g->m_mainMenu.m_background.SetAlpha(0xff);
            frames(2);
        }
        g->SetPanelMode(Game::PANEL_HORIZONTAL);
        const float aspect = h.CanvasAspect();
        Check(aspect > 1.97f && aspect < 1.975f && g->m_viewportWidth == 640 && g->m_viewportHeight == 324,
              "Horizontal: a 2.175 x 1.103 canvas (640 x 324 in a 640 x 960 view)");
        frames(2);
        Check(g->m_layout == Game::LAYOUT_FIT, "the title screen is the original screen, centred");
        float gx = 0, gy = 0;
        const float vh = static_cast<float>(g->m_viewportHeight), vw = static_cast<float>(g->m_viewportWidth);
        g->MapPointer(0.5f, 0.5f / 1.5f * (vh / vw), gx, gy);   // the view's centre
        Check(gx > 0.49f && gx < 0.51f && gy > 0.74f && gy < 0.76f, "the view's centre maps to the screen's centre");

        g->InitNewGame();   // a game on the board
        g->m_nameScreen.m_active = false;
        g->m_mainMenu.FadeOut();   // as Continue: the title fades out over the board
        frames(2);
        Check(g->m_mainMenu.m_active && g->m_layout == Game::LAYOUT_SPLIT,
              "the board is already split while the title screen fades out over it");
        g->m_mainMenu.m_active = false;
        frames(60);
        Check(g->m_layout == Game::LAYOUT_SPLIT, "a game: the split layout");
        // a canvas point (canvas units, kWideW wide) in MapPointer's units
        auto mapCanvas = [&](float cx, float cy) { g->MapPointer(cx / Game::kWideW, cy / (1.5f * Game::kWideW), gx, gy); };
        mapCanvas(Game::kFrameL + 0.5f, Game::kFrameT + 0.75f - Game::kWideBoardTop);   // the board card
        Check(gx > 0.49f && gx < 0.51f && gy > 0.74f && gy < 0.76f, "the left card maps to the board");
        const float rx = Game::kWideColW + Game::kFrameL + 0.5f;
        mapCanvas(rx, Game::kFrameT + 0.1f);
        Check(gx > 0.49f && gx < 0.51f && gy > 0.09f && gy < 0.11f, "the right card's top maps to the skills bar");
        mapCanvas(rx, Game::kFrameT + Game::kWideSkillsH + 0.1f);
        Check(gy > 1.35f && gy < 1.37f, "under the skills bar is the HUD");
        mapCanvas(rx, Game::kWideCardH + Game::kFrameT + 0.2f);   // the panel card
        g->m_hoverActive = true;
        g->m_hoverX = gx;
        g->m_hoverY = gy;
        std::vector<InfoPanel::Line> lines = g->m_infoPanel.BuildLines();
        Check(gy > 1.69f && gy < 1.71f && !lines.empty() && lines[0].text.rfind("Enemies on the board: ", 0) == 0,
              "the lower card is the info panel (the mouse there: the status)");
        g->PointerLeave();
        for (int i = 0; i < 4; ++i) g->AddToken(&g->m_board[1][i], i % 2 ? TILE_COIN : TILE_SKULL, false, nullptr);
        frames(10);   // tokens in flight: drawn gliding to the right column
        Check(g->m_layout == Game::LAYOUT_SPLIT, "tokens fly in the split layout");
        frames(120);   // the tokens arrive
        Check(h.Back(), "Esc on the board does not quit");
        frames(15);
        Check(g->m_interface.m_gameMenu.m_active && !g->m_interface.m_gameMenu.m_closing && !g->m_mainMenu.m_active,
              "Esc on the board opens the pause menu (not the title screen)");
        h.Back();
        frames(15);
        Check(!g->m_interface.m_gameMenu.m_active, "Esc again closes the pause menu");
        g->m_interface.m_gameMenu.Show();
        frames(2);
        g->MapPointer(0.5f, 0.5f / 1.5f * (vh / vw), gx, gy);
        Check(g->m_layout == Game::LAYOUT_FIT && gx > 0.49f && gx < 0.51f && gy > 0.74f && gy < 0.76f,
              "a menu over the board: centred, as the title screen (the board stays split)");
        g->m_interface.m_gameMenu.m_active = false;
        g->m_mainMenu.m_active = true;   // back to the title screen (no more game frames)
        frames(2);
    }
    {
        host::Host h(platform, config);
        Check(h.Init(640, 960) && h.GetGame()->IsWide(), "Horizontal stays on after a restart");
        Check(h.Fullscreen(), "Fullscreen stays on after a restart");
        config.desktop = false;
        h.GetGame()->SetPanelMode(Game::PANEL_TOP);   // (leave the saved settings at the default)
        h.SetFullscreen(false);
        h.Frame();
        GlTraceReset();
    }
    {
        host::Host h(platform, config);   // not a desktop (Android): no Fullscreen setting
        Check(h.Init(640, 960), "Init (not a desktop)");
        h.SetFullscreen(true);
        h.Frame();
        GlTraceReset();
        Check(!h.Fullscreen(), "not a desktop: Fullscreen stays off");
    }
    // Settings from before the Info panel setting: Panel at bottom, Horizontal (desktop only).
    {
        const fs::path ini = scratch / "settings.ini";
        auto writeIni = [&](const char* extra) {
            std::FILE* f = std::fopen(ini.string().c_str(), "w");
            if (f) {
                std::fputs(extra, f);
                std::fclose(f);
            }
        };
        writeIni("panelbottom=true\nhorizontal=true\n");
        {
            host::Host h(platform, config);   // not a desktop
            Check(h.Init(640, 960) && h.GetGame()->m_panelMode == Game::PANEL_BOTTOM,
                  "old settings, not a desktop: Panel at bottom becomes Bottom (no Horizontal)");
        }
        config.desktop = true;
        writeIni("panelbottom=true\nhorizontal=true\n");
        {
            host::Host h(platform, config);
            Check(h.Init(640, 960) && h.GetGame()->m_panelMode == Game::PANEL_HORIZONTAL,
                  "old settings, desktop: Horizontal becomes Horizontal");
        }
        config.desktop = false;
        writeIni("");
        {
            host::Host h(platform, config);
            Check(h.Init(640, 960) && h.GetGame()->m_panelMode == Game::PANEL_TOP, "no setting: Top");
        }
    }

    std::printf("%s\n", g_failures == 0 ? "all checks passed" : "FAILED");
    return g_failures == 0 ? 0 : 1;
}
