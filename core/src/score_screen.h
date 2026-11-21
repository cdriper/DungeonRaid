// ScoreScreen: the high score tables (Easy, Normal, Hard, Harder, Dungeon Sprint, Pretzel Hero; ten
// entries each), shown three pages at a time (previous, current, next) that slide with a swipe or
// the arrow buttons; the newest entry highlighted. Keeps the tables (AddScore, SetSkills) and
// serialises them for the Java shell. 0xf4c bytes in the original, embedded in Game at 0xf454; the
// layout below follows it. ScorePage (0x301c bytes, heap) holds one page's quads and labels.
#pragma once

#include "game_state.h"
#include "label.h"
#include "quad.h"
#include "quad_strip.h"

class Game;

struct ScorePage {
    ScorePage();                                        // 0x00024044
    QuadStrip m_rankPanels[10];                         // 0x0
    QuadStrip m_rowPanels[10];                          // 0x2f8
    Label m_rowLabels[10][7];                           // 0x5f0: rank, name, turns, XP, gold, upgrades, score
    Quad m_skillIcons[10][4];                           // 0x1ab8
    Label m_headers[5];                                 // 0x2cd8: Turns, XP, GP, UP (or Avg, Max, Total), Score
    QuadStrip m_prevButton;                             // 0x2e54
    QuadStrip m_prevArrow;                              // 0x2ea0
    QuadStrip m_nextButton;                             // 0x2eec
    QuadStrip m_nextArrow;                              // 0x2f38
    QuadStrip m_titlePanel;                             // 0x2f84
    Label m_title;                                      // 0x2fd0
    Label m_dates[10];                                  // port: each row's date (dd.mm.yy), left of the skill icons
};

class ScoreScreen {
public:
    static constexpr int kTypes = 6;     // ScoreType: 0..3 difficulties, 4 Sprint, 5 Pretzel
    static constexpr int kEntries = 10;

    struct Entry {                                      // 0x38 bytes
        char name[20];
        int turn;
        int a;                                          // XP (Pretzel: average length x 10)
        int b;                                          // gold (max length)
        int c;                                          // upgrades (total pretzels)
        int score;
        int skills[4];                                  // SkillType, -1 none
        uint32_t time;                                  // port: when the score was added (Unix seconds), 0 unknown
    };

    explicit ScoreScreen(Game* game);                   // 0x0005e6f8
    ~ScoreScreen();                                     // 0x0005e650
    ScoreScreen(const ScoreScreen&) = delete;
    ScoreScreen& operator=(const ScoreScreen&) = delete;

    void Initialise();                                  // 0x0005d5dc: the pages' headers and rows
    void CreatePage(int page);                          // 0x0005e2ec
    void InitialisePage(int page);                      // 0x0005d0e0
    void Show(bool instant);                            // 0x0005da94
    void Show(int type, bool instant);                  // 0x0005db40 (ScoreType)
    void ShowMode(int mode, bool instant);              // 0x0005db50 Show(GameMode, bool)
    void ShowDifficulty(int difficulty, bool instant);  // 0x0005db70 Show(Difficulty, bool)
    void FadeOut();                                     // 0x0005c444
    void SetAlpha(uint8_t alpha);                       // 0x0005d8e8
    void Process(float dt);                             // 0x0005dba8
    void Render();                                      // 0x0005e204
    void RenderPage(int page);                          // 0x0005e070
    void AddScore(const char* name, int turn, int a, int b, int c, int score, int type);   // 0x0005cc5c
    void SetSkills(int s0, int s1, int s2, int s3);     // 0x0005c398: of the entry last added
    int GetScoreCount() const;                          // 0x0005c404
    bool HaveScoresChanged() const;                     // 0x0005c428
    void SerialiseState(int type);                      // 0x0005cb34
    void SerialiseChangedState();                       // 0x0005cc3c: the first changed table
    void DeserialiseState(int version, int type);       // 0x0005ca10 (version: the file's length; port: the dates after the entries)
    void DeserialiseLegacyState();                      // 0x0005cec4

    static void SetLabelText(Label& label, const char* text, float right);   // 0x0005d020
    static void SetLabelNumber(Label& label, int value, float x, bool left, int divisor);   // 0x0005d04c

    Game* m_game;                                       // 0x0
    bool m_active = false;                              // 0x4
    bool m_closing = false;                             // 0x5 (unset by the constructor in the original, as the scalars below)
    float m_fadeTime = 0.0f;                            // 0x8
    Quad m_background;                                  // 0xc
    Quad m_panelBackground;                             // 0x80
    Quad m_banner;                                      // 0xf4
    int m_lastTouchState = 0;                           // 0x168
    bool m_swiping = false;                             // 0x16c
    float m_lastX = 0.0f;                               // 0x170
    float m_startX = 0.0f;                              // 0x174
    float m_velocity = 0.0f;                            // 0x178
    float m_offset = 0.0f;                              // 0x17c: the pages' slide offset
    int m_unused180 = 0;                                // 0x180 (Show sets it to 0, nothing reads it)
    bool m_sliding = false;                             // 0x184
    int m_slideDirection = 0;                           // 0x188: 1 to the previous page, -1 to the next, 0 back
    QuadStrip m_backButton;                             // 0x18c
    bool m_backPressed = false;                         // 0x1d8
    bool m_prevPressed = false;                         // 0x1d9
    bool m_nextPressed = false;                         // 0x1da
    int m_type = 0;                                     // 0x1dc: the current page's table
    ScorePage* m_pages[3] = {};                         // 0x1e0: previous, current, next
    Entry m_entries[kTypes][kEntries] = {};             // 0x1ec
    int m_counts[kTypes] = {};                          // 0xf0c
    int m_lastType = -1;                                // 0xf24: the entry last added
    int m_lastIndex = -1;                               // 0xf28
    GameState m_state{0x400};                           // 0xf2c
    int m_serialisedType = 0;                           // 0xf40
    bool m_changed[kTypes] = {};                        // 0xf44

private:
    // The word the original reads at 0xf08 + 4 k as table k - 1's count: k 0 is the last entry's
    // last skill, k 7 m_lastType (pages beyond the first and last table).
    int CountWord(int k) const;
};
