// GameMenu: the in-game options menu behind the HUD's menu button (help, sound, help pages, clock,
// attack dimming, smooth framerate, back to the main menu). 0x94 bytes in the original, embedded in
// Interface at 0xf08; the layout below follows it (offsets in the comments).
//
// Port: the settings moved to SettingsScreen. The menu shows help, Settings (quad 10, the sound
// button's place, with a generated "Settings" image) and main menu, in a shorter frame; quads 11..14
// and the tick boxes 16..20 stay in the layout but are neither drawn nor pressed. In debug mode a
// fourth button, Debug (quad 21; a port-only image at 912,350 in texture 1, made from the other
// buttons' letters), above Exit (main menu), opens the settings screen's Debug page; the frame grows
// by a button row for it.
#pragma once

#include "menu.h"
#include "quad.h"

class Game;

class GameMenu : public Menu {
public:
    // Quads: 0..8 frame, 9..15 buttons (help, sound, help pages, clock, dimming, smooth, main
    // menu), 16..20 the tick boxes of sound, help pages, clock, dimming, smooth; port: 21 Debug.
    static constexpr int kQuadCount = 0x16;             // 0x15 in the original (+ Debug)
    static constexpr int kDebugQuad = 21;

    explicit GameMenu(Game* game);                      // 0x00044880
    ~GameMenu() override = default;                     // 0x0004481c

    void Show();                                        // 0x0004430c
    void Process(float dt);                             // 0x00044564
    void Render();                                      // 0x000447ac
    bool CursorInQuad(int quad) const;                  // 0x000442a4
    void SetSoundState(bool on);                        // 0x000444f8
    void SetHelpState(bool on);                         // 0x00044490
    void SetClockState(bool on);                        // 0x00044428
    void SetDimmingState(bool on);                      // 0x000443c0
    void SetSmoothState(bool on);                       // 0x00044358

    Game* m_game;                                       // 0x8
    bool m_active = false;                              // 0xc
    int m_lastTouchState = 0;                           // 0x10 (unset by the constructor in the original)
    int m_pressed = 0;                                  // 0x14: button being pressed, -1 none (likewise)
    bool m_closing = false;                             // 0x18 (likewise)
    float m_fadeTime = 0.0f;                            // 0x1c: fade time left (0.2 s), 0 = idle (likewise)
    Quad m_shade;                                       // 0x20: darkens the screen
    bool m_debugShown = false;                          // port: the Debug button (debug mode, at Show)

private:
    void SetTick(int quad, bool on);
    void LayoutFrame(bool debug);                       // port: the frame, one button row taller with Debug (above Exit)
};
