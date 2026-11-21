// InfoPanel (port-only, not in the original): the strip above the board on the tall (9:16) canvas
// (below the HUD with the "Panel at bottom" setting), Game::kPanelHeight tall, up to 4 lines of text (5 in a smaller size when a description
// would be cut; an enemy's stats share the line with its name) about, in this order:
//  - the chain being made: what it gives (health, armour, upgrade points, gold, XP), and for an
//    attack chain the damage, the enemies killed and (red) the ones that survive, and the enemies'
//    attack after it;
//  - what the mouse is over (desktop): a tile (enemy stats, a special enemy's description, what a
//    tile gives) or a skill button (level, charge, description);
//  - touches (both platforms): while a skill button is pressed, that skill (slide off to not cast
//    it); while a chain has at most one tile, the tile under the finger. A tap (released with at
//    most one tile) or a skill press released without casting keeps that info in the panel until
//    the next touch, a cast, a chain of 2+ tiles or a new turn;
//  While a menu is open over the board (level-up, shop, upgrade, pause, stats) only the status.
//  - the status: the enemies on the board, their total attack and what reaches the health after
//    the armour (not the health and armour themselves: the HUD shows them); then the turn, the
//    turns to the next special enemy and the special kills to the next trophy.
// The text is rebuilt every frame from the game's state, read only (no game state changes, no
// random numbers); the labels are laid out again only when the text changes.
#pragma once

#include <string>
#include <vector>

#include "dr_types.h"
#include "label.h"
#include "quad.h"

class Game;
class Tile;

class InfoPanel {
public:
    explicit InfoPanel(Game* game);
    InfoPanel(const InfoPanel&) = delete;
    InfoPanel& operator=(const InfoPanel&) = delete;

    void Render();

    // one line: text with coloured spans (a span covers [from, to) of the text)
    struct Span {
        unsigned from, to;
        GLcolour4 colour;
    };
    struct Line {
        std::string text;
        std::vector<Span> spans;
        Line& Add(const std::string& part, GLcolour4 colour);
        bool operator==(const Line& o) const;
    };
    // The lines the panel would show now (also for the tests), at the largest text size at which
    // nothing is cut (4 lines at the normal size, 5 at two smaller ones); TextScale() is that size.
    std::vector<Line> BuildLines();
    float TextScale() const { return m_scale; }

private:
    // what a touch began on / the info kept after a tap
    struct Pick {
        enum Kind { NONE, TILE, SKILL } kind = NONE;
        int row = 0, col = 0;     // TILE
        int skill = -1;           // SKILL: index into Game::m_skills
    };
    Pick PickAt(const GLfloat2& p);
    void Track();                  // follows the touches (press, release, the turn)
    bool BuildPick(std::vector<Line>& lines, const Pick& pick);
    void BuildContent(std::vector<Line>& lines);   // touch, hover, kept info or status, at m_scale
    float PanelTop() const;                         // board coordinates: 1.5 (below) or -height (above)
    float PanelHeight() const;                      // Game::kPanelHeight, or taller on the horizontal layout
    bool InPanel(float y) const;
    void BuildSelection(std::vector<Line>& lines);
    bool BuildHover(std::vector<Line>& lines);   // false: nothing under the mouse
    void BuildSkill(std::vector<Line>& lines, int index);
    void BuildTile(std::vector<Line>& lines, Tile* t);
    void BuildStatus(std::vector<Line>& lines);
    void AddTurnLines(std::vector<Line>& lines);   // the turn, the next special enemy and trophy
    void AddWrapped(std::vector<Line>& lines, const char* text, GLcolour4 colour, size_t maxLines);
    float TextWidth(const std::string& text);
    void Layout(const std::vector<Line>& lines);

    Game* m_game;
    Quad m_background;
    Quad m_edge;                  // horizontal layout: a line on the HUD's side (the card has its frame)
    Quad m_frame[4];              // tall canvas: a thin frame around the panel (top, bottom, left, right)
    Label m_measure;              // for the word wrap
    std::vector<Label> m_labels;
    std::vector<Line> m_shown;
    bool m_laidOut = false;
    // the text size being tried / laid out, its line limit, and whether something was cut at it
    float m_scale = 0.078f;
    float m_shownScale = 0.0f;
    float m_shownTop = 0.0f;
    size_t m_maxLines = 4;
    bool m_cut = false;
    // touch tracking (cosmetic state of the panel, not the game's)
    Pick m_press;                  // what the current touch began on
    Pick m_kept;                   // shown after a tap until something else happens
    int m_pressCharge = 0;         // the pressed skill's charge at the press (lower after: it was cast)
    size_t m_maxChain = 0;         // the longest the chain got during the touch
    GLfloat2 m_lastTouch{0.0f, 0.0f};
    bool m_wasTouching = false;
    int m_turn = -1;
};
