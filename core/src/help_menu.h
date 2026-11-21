// HelpMenu: the help pages, a speech bubble pointing at what it explains (above or below it), shown
// once per page while help is on; a tap on the bubble closes it, then the next queued text shows.
// 0x44 bytes in the original, embedded in Game at 0x103a0; the layout below follows it.
#pragma once

#include <list>
#include <string>
#include <vector>

#include "label.h"
#include "menu.h"

class Game;
class GameState;

class HelpMenu : public Menu {
public:
    // Quads: 0..8 the bubble's frame (the top or bottom edge split around the pointer), 9/10 the
    // rest of the top/bottom edge, 11/12 the pointer up/down (the unused ones parked at 1000).
    static constexpr int kQuadCount = 0xd;
    static constexpr int kPageCount = 0x17;

    explicit HelpMenu(Game* game);                      // 0x00047954
    ~HelpMenu() override = default;                     // 0x00047d94

    // Page `help` (g_helpText) at the point (x, y) of a thing of size (w, h), if it is still to show
    // and no page is open; the last page switches help off.
    void Initialise(float x, float y, float w, float h, int help);   // 0x00048348
    void Show(const char* text, float x, float y, float w, float h); // 0x00048190
    void QueueHelp(const char* text, float x, float y, float w);    // 0x00048514
    void Process(float dt);                             // 0x000483a4
    void Render();                                      // 0x000478d4
    void InitialiseLabels(const char* text, float width);   // 0x00047eb4
    void SetSize(float w, float h, float pointer, bool below);   // 0x000474b0
    void FadeOut();                                     // 0x0004747c
    void ResetHelp();                                   // 0x00047458: every page to show again
    void ClearHelp();                                   // 0x00047474
    void Serialise(GameState* state);                   // 0x000474a0
    void Deserialise(GameState* state);                 // 0x00047490

    struct QueuedHelp {
        std::string text;
        float x, y, w;
    };

    Game* m_game;                                       // 0x8
    bool m_active = false;                              // 0xc
    bool m_closing = false;                             // 0xd (unset by the constructor in the original, as the scalars below)
    float m_fadeTime = 0.0f;                            // 0x10 (0.2 s)
    GLfloat2 m_position{0.0f, 0.0f};                    // 0x14: top left
    float m_width = 0.0f;                               // 0x1c (0.98)
    float m_height = 0.0f;                              // 0x20
    int m_lastTouchState = 0;                           // 0x24
    std::vector<Label> m_labels;                        // 0x28: grows as needed, never shrinks
    int m_labelCount = 0;                               // 0x34
    int m_shownHelp = 0;                                // 0x38: bit mask of the pages still to show; serialised
    std::list<QueuedHelp> m_queue;                      // 0x3c
};
