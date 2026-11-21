// AchievementsScreen (port-only, not in the original, whose achievements were on OpenFeint): the
// list of all achievements, opened from the title screen. A titled panel (in the style of
// SettingsScreen) with one row per achievement, the unlocked ones first (newest first), then the
// locked ones in the list's order: a tick box, the title, the description (wrapped to two lines)
// and, right, the unlock date or the progress of a counting one ("37%"); locked rows are dimmer. The list scrolls (a drag on it, flung on with a finger; the mouse wheel; the bar on
// the right: its thumb follows a drag, and with the mouse a press beside the thumb jumps there; a
// finger grabs the thumb only, so swipes at the edge move the list), clipped to the panel. Below
// it the number unlocked and a Back button. Fades in over the title screen and back out; while
// open it takes the frames and the back key (Game::Process, Game::BackPressed).
#pragma once

#include <vector>

#include "label.h"
#include "quad.h"
#include "quad_strip.h"

class Game;

class AchievementsScreen {
public:
    explicit AchievementsScreen(Game* game);
    AchievementsScreen(const AchievementsScreen&) = delete;
    AchievementsScreen& operator=(const AchievementsScreen&) = delete;

    void Show();       // fades in at the top of the list (the fonts must be loaded)
    void FadeOut();    // fades out, then inactive
    void Process(float dt);
    void Render();
    void Scroll(float amount);   // screen units down (the mouse wheel)
    const Quad& Background() const { return m_background; }
    int AchievementAt(size_t row) const { return row < m_rows.size() ? m_rows[row].achievement : 0; }   // (as laid out on Show)

    bool m_active = false;
    float m_scroll = 0.0f;       // how far the list is scrolled (screen units)
    float m_maxScroll = 0.0f;

private:
    struct Row {
        QuadStrip button;
        Quad tick;
        Label title;
        Label lines[2];          // the description, wrapped
        Label state;             // the date or the progress
        float top = 0.0f;        // in the list (unscrolled)
        int achievement = 0;     // which one (the rows are sorted)
        bool unlocked = false;
    };

    void Layout();               // the rows from the Game's records (on Show)
    void SetAlpha(uint8_t alpha);
    void SetScroll(float scroll);
    void UpdateBar();
    float ThumbHeight() const;
    bool OnBar(const GLfloat2& p) const;   // a press grabs the bar (a reach around it)
    Label MakeLabel(float scale) const;

    Game* m_game;
    bool m_closing = false;
    float m_fadeTime = 0.0f;
    int m_lastTouchState = 0;
    bool m_backPressed = false;
    bool m_dragging = false;
    bool m_draggingBar = false;  // the drag holds the scroll bar's thumb (moves the other way)
    float m_lastScroll = 0.0f;   // a finger drag: the scroll at the previous frame
    float m_velocity = 0.0f;     // its speed (screen units per second, smoothed)
    float m_fling = 0.0f;        // after the finger lifts: the list glides on at this speed, slowing
    float m_dragStartY = 0.0f;
    float m_dragStartScroll = 0.0f;
    uint8_t m_alpha = 0;
    Quad m_background;           // black, the whole screen
    Quad m_panelBackground;      // dark blue, above the Back button
    QuadStrip m_panel;
    Label m_heading;
    Label m_count;               // "12 / 77 unlocked", left of Back
    QuadStrip m_backButton;
    float m_listTop = 0.0f;      // the visible part of the list (screen units)
    float m_listBottom = 0.0f;
    float m_listLeft = 0.0f;
    float m_listRight = 0.0f;
    float m_contentHeight = 0.0f;
    Quad m_barTrack;             // untextured
    Quad m_barThumb;
    std::vector<Row> m_rows;
};
