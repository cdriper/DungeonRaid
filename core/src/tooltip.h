// Tooltip: the HUD's info box (enemy, skill or scroll descriptions): a 9-quad frame, a yellow title
// and the description split into lines; fades in and out. 0x78 bytes in the original, embedded in
// Interface at 0x114c; the layout below follows it (offsets in the comments).
#pragma once

#include <vector>

#include "dr_types.h"
#include "label.h"
#include "menu.h"

class Font;
class Game;

class Tooltip : public Menu {
public:
    explicit Tooltip(Game* game);                                   // 0x00065b04
    ~Tooltip() override = default;                                  // 0x00065a90

    void SetSize(float w, float h);                                 // 0x00065620: the frame for w x h
    // positions the box at `position` (below it, or above it from y > 1, shifted by `offset` pixels)
    // and lays out the labels
    void Show(const GLfloat2& position, int offset);                // 0x00065764
    void Show(EnemyType type, const GLfloat2& position);            // 0x000662f0
    void Show(SkillType type, int cooldown, int charge, const GLfloat2& position);   // 0x00066400
    void ShowScroll(SkillType type, const GLfloat2& position);      // 0x00066270
    void InitialiseLabels(const char* title, const char* text, GLcolour4 colour);    // 0x000661a8
    void AddLabel(const char* text, GLcolour4 colour);              // 0x00065f64
    void Hide();                                                    // 0x00065610: fades out
    void HideInstantly();                                           // 0x000658dc
    void Process(float dt);                                         // 0x000659c0: the fade
    void Render();                                                  // 0x00065938

    Game* m_game;                       // 0x8
    const Font* m_font;                 // 0xc (Game's first font)
    Label m_title;                      // 0x10
    std::vector<Label> m_labels;        // 0x5c: the description lines (grows, never shrinks)
    int m_labelCount = 0;               // 0x68: lines in use
    bool m_visible = false;             // 0x6c
    bool m_hiding = false;              // 0x6d
    GLfloat2 m_position{0.0f, 0.0f};    // 0x70 (unset by the constructor in the original)
};
