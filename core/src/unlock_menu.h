// UnlockMenu: the "Class unlocked" / "level up" box (and the bonus prompts): title, icon, wrapped
// description; fades in, a tap closes it. 0xf4 bytes in the original, embedded in Game at 0x15d20;
// the layout below follows it.
#pragma once

#include <vector>

#include "label.h"
#include "menu.h"
#include "quad.h"

class Font;
class Game;

class UnlockMenu : public Menu {
public:
    static constexpr int kQuadCount = 9;   // the frame

    explicit UnlockMenu(Game* game);                    // 0x00066878
    ~UnlockMenu() override = default;                   // 0x000667f4

    void Show(const char* title, const char* text);     // 0x00066fc4
    void Show(int classType, int level);                // 0x000671ec (ClassType)
    void ShowPromo(const char* title, const char* text);   // 0x000671d8
    void Hide();                                        // 0x00066488
    void HideInstantly();                               // 0x000665dc
    void Process(float dt);                             // 0x000666e0
    void Render();                                      // 0x00066640
    void SetSize(float w, float h);                     // 0x00066498
    void InitialiseLabels(const char* text);            // 0x00066d10

    Game* m_game;                                       // 0x8
    const Font* m_font;                                 // 0xc
    Label m_title;                                      // 0x10
    std::vector<Label> m_labels;                        // 0x5c: grows as needed, never shrinks
    int m_labelCount = 0;                               // 0x68
    Quad m_icon;                                        // 0x6c
    bool m_active = false;                              // 0xe0
    bool m_hiding = false;                              // 0xe1
    bool m_promo = false;                               // 0xe2: a bonus prompt (Game::BonusPromptClosed when closed)
    GLfloat2 m_position{0.0f, 0.0f};                    // 0xe4 (unset by the constructor in the original, as below)
    int m_lastTouchState = 0;                           // 0xec
    int m_iconTexture = 0;                              // 0xf0: 4 class icons, 1 the promo
};
