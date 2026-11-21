// IntroScreen: the story page before a new game (one of 61 texts, wrapped), with "Begin" to tap;
// fades in over a black screen and out again. 0xe4 bytes in the original, embedded in Game at
// 0xc6f8; the layout below follows it.
#pragma once

#include <vector>

#include "label.h"
#include "quad.h"

class Game;

class IntroScreen {
public:
    explicit IntroScreen(Game* game);                   // 0x0004b580
    ~IntroScreen() = default;                           // 0x0004b540
    IntroScreen(const IntroScreen&) = delete;
    IntroScreen& operator=(const IntroScreen&) = delete;

    void Initialise();                                  // 0x0004b668: a random text, fading in
    void FadeOut();                                     // 0x0004b340
    void Process(float dt);                             // 0x0004b35c
    void Render();                                      // 0x0004b4f8

    Game* m_game;                                       // 0x0
    float m_unused = -1.0f;                             // 0x4 (set by the constructor, never read)
    Quad m_shade;                                       // 0x8: black behind the text
    std::vector<Label> m_lines;                         // 0x7c: grows as needed, never shrinks
    Label m_begin;                                      // 0x88
    int m_lineCount = 0;                                // 0xd4 (unset by the constructor in the original)
    bool m_active = false;                              // 0xd8
    bool m_closing = false;                             // 0xd9
    float m_time = 0.0f;                                // 0xdc: fade 0..1 (likewise unset)
    int m_lastTouchState = 0;                           // 0xe0
};
