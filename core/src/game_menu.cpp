#include "game_menu.h"

#include "dr_gl.h"
#include "game.h"
#include "util.h"

namespace {
// Port: the settings buttons and tick boxes that moved to SettingsScreen (quads 11..14, 16..20);
// the Debug button (quad 21) is drawn and pressed only in debug mode.
bool Hidden(int quad) { return (quad >= 11 && quad <= 14) || quad >= 16; }
}  // namespace

GameMenu::GameMenu(Game* game) : Menu(kQuadCount), m_game(game) {
    m_shade.SetSize(GLfloat2{2.0f, 2.0f});
    m_shade.m_position = GLfloat2{0.5f, 0.5f};
    m_shade.SetColour(GLcolour4{0, 0, 0, 0});
    // the frame: 13-pixel corners, the middle stretched to 99 x 104 (port; 216 in the original)
    SetQuad(0, {98.0f, 105.0f}, {498.0f, 498.0f}, {13.0f, 13.0f}, 1.0f, 1.0f);
    SetQuad(1, {111.0f, 105.0f}, {479.0f, 472.0f}, {1.0f, 13.0f}, 99.0f, 1.0f);
    SetQuad(2, {210.0f, 105.0f}, {380.0f, 415.0f}, {15.0f, 13.0f}, 1.0f, 1.0f);
    LayoutFrame(false);
    // the buttons
    const GLfloat2 button{89.0f, 26.0f};
    SetQuad(9, {116.0f, 123.0f}, {329.0f, 364.0f}, button, 1.0f, 1.0f);
    SetQuad(10, {116.0f, 157.0f}, {820.0f, 350.0f}, button, 1.0f, 1.0f);   // port: Settings
    SetQuad(11, {116.0f, 185.0f}, {422.0f, 374.0f}, button, 1.0f, 1.0f);
    SetQuad(12, {116.0f, 213.0f}, {647.0f, 997.0f}, button, 1.0f, 1.0f);
    SetQuad(13, {116.0f, 241.0f}, {737.0f, 997.0f}, button, 1.0f, 1.0f);
    SetQuad(14, {116.0f, 269.0f}, {388.0f, 997.0f}, button, 1.0f, 1.0f);
    SetQuad(15, {116.0f, 191.0f}, {422.0f, 401.0f}, button, 1.0f, 1.0f);   // port: 303 in the original
    // the tick boxes (unticked)
    for (int i = 0; i < 5; ++i) {
        SetQuad(16 + i, {121.0f, 162.0f + 28.0f * static_cast<float>(i)}, {405.0f, 411.0f}, {16.0f, 16.0f}, 1.0f, 1.0f);
    }
    // port: Debug, the same 34-pixel step; its image is at 912,350 in texture 1. Shown, it takes
    // Exit's place and Exit moves a row down (LayoutFrame)
    SetQuad(kDebugQuad, {116.0f, 191.0f}, {912.0f, 350.0f}, button, 1.0f, 1.0f);
}

// The frame's middle and bottom: the middle is 104 pixels tall (help, Settings, main menu), 138 with
// the Debug button, which then goes above Exit (the main menu button, a row down).
void GameMenu::LayoutFrame(bool debug) {
    SetQuad(15, 116.0f, debug ? 225.0f : 191.0f, 89.0f, 26.0f);   // positions only
    SetQuad(kDebugQuad, 116.0f, 191.0f, 89.0f, 26.0f);
    const float h = debug ? 138.0f : 104.0f;
    const float bottom = 118.0f + h;
    SetQuad(3, {98.0f, 118.0f}, {478.0f, 487.0f}, {13.0f, 1.0f}, 1.0f, h);
    SetQuad(4, {111.0f, 118.0f}, {145.0f, 399.0f}, {1.0f, 1.0f}, 99.0f, h);
    SetQuad(5, {210.0f, 118.0f}, {380.0f, 430.0f}, {15.0f, 1.0f}, 1.0f, h);
    SetQuad(6, {98.0f, bottom}, {498.0f, 482.0f}, {13.0f, 15.0f}, 1.0f, 1.0f);
    SetQuad(7, {111.0f, bottom}, {483.0f, 470.0f}, {1.0f, 15.0f}, 99.0f, 1.0f);
    SetQuad(8, {210.0f, bottom}, {380.0f, 433.0f}, {15.0f, 13.0f}, 1.0f, 1.0f);
}

void GameMenu::Show() {
    m_fadeTime = 0.2f;
    m_lastTouchState = 0;
    m_active = true;
    m_closing = false;
    m_pressed = -1;
    m_shade.SetAlpha(0);
    for (int q = 0; q < kQuadCount; ++q) SetQuadColour(q, 0xff, 0xff, 0xff, 0);
    m_debugShown = m_game->m_debugMode;   // port
    LayoutFrame(m_debugShown);
}

// While fading (in, or out after "help"/a close), the shade and quads fade; otherwise touches: a
// press on a button highlights it, releasing on it toggles its setting.
void GameMenu::Process(float dt) {
    float t = 0.0f;
    if (m_fadeTime <= 0.0f) {
        Game* g = m_game;
        int touch = g->m_touchState;
        if (touch == TOUCHING) {
            if (m_lastTouchState != NOT_TOUCHING) {
                m_lastTouchState = touch;
                return;
            }
            for (int q = 9; q < 16; ++q) {
                if (!Hidden(q) && CursorInQuad(q)) {
                    SetQuadColour(q, 0xff, 0xff, 0xff, 0x96);
                    m_pressed = q;
                    g->QueueSound(SOUND_CLICK);
                    break;
                }
            }
            if (m_pressed < 0 && m_debugShown && CursorInQuad(kDebugQuad)) {   // port
                SetQuadColour(kDebugQuad, 0xff, 0xff, 0xff, 0x96);
                m_pressed = kDebugQuad;
                g->QueueSound(SOUND_CLICK);
            }
            m_lastTouchState = g->m_touchState;
            return;
        }
        if ((touch != FINISHED_TOUCHING && touch != CANCELLED_TOUCHING) || m_pressed < 0) {
            m_lastTouchState = touch;
            return;
        }
        SetQuadColour(m_pressed, 0xff, 0xff, 0xff, 0xff);
        if (CursorInQuad(m_pressed)) {
            switch (m_pressed) {
                case 9:   // help (closes the menu)
                    m_fadeTime = 0.2f;
                    m_closing = true;
                    g->UpdateHelpState();
                    break;
                case 10: g->m_settingsScreen.Show(); break;   // port: the settings (the sound toggle in the original)
                case 15: g->FadeInMainMenu(); break;
                case kDebugQuad: g->m_settingsScreen.Show(1); break;   // port: the Debug page (over this menu)
                default: break;
            }
        }
        m_pressed = -1;
        m_lastTouchState = m_game->m_touchState;
        return;
    }
    float left = m_fadeTime - dt;
    m_fadeTime = left;
    if (left > 0.0f) {
        t = left / 0.2f;
    } else {
        m_fadeTime = 0.0f;
        if (m_closing) m_active = false;
    }
    float from;
    float to;
    if (m_closing) {
        m_shade.SetAlpha(static_cast<uint8_t>(FloatToUintArm(Interpolate(180.0f, 0.0f, t))));
        from = 255.0f;
        to = 0.0f;
    } else {
        m_shade.SetAlpha(static_cast<uint8_t>(FloatToUintArm(Interpolate(0.0f, 180.0f, t))));
        from = 0.0f;
        to = 255.0f;
    }
    uint8_t alpha = static_cast<uint8_t>(FloatToIntArm(Interpolate(from, to, t)));
    for (int q = 0; q < kQuadCount; ++q) SetQuadColour(q, 0xff, 0xff, 0xff, alpha);
}

void GameMenu::Render() {
    sdlPushMatrix();
    sdlDisable(GL_TEXTURE_2D);
    m_shade.Render();
    sdlEnable(GL_TEXTURE_2D);
    sdlBindTexture(GL_TEXTURE_2D, 1);
    sdlVertexPointer(2, GL_FLOAT, sizeof(QuadVertex), &m_vertices[0].position);
    sdlColorPointer(4, GL_UNSIGNED_BYTE, sizeof(QuadVertex), &m_vertices[0].colour);
    sdlTexCoordPointer(2, GL_FLOAT, sizeof(QuadVertex), &m_vertices[0].uv);
    for (int q = 0; q < kQuadCount; ++q) {
        if (!Hidden(q)) sdlDrawArrays(GL_TRIANGLE_FAN, q * 4, 4);
    }
    if (m_debugShown) sdlDrawArrays(GL_TRIANGLE_FAN, kDebugQuad * 4, 4);   // port
    sdlPopMatrix();
}

bool GameMenu::CursorInQuad(int quad) const {
    const QuadVertex* v = &m_vertices[quad * 4];
    float tx = m_game->m_touchX;
    float ty = m_game->m_touchY;
    return v[0].position.x < tx && tx < v[2].position.x && v[0].position.y < ty && ty < v[2].position.y;
}

void GameMenu::SetTick(int quad, bool on) {
    SetQuadUV(quad, on ? GLfloat2{405.0f, 394.0f} : GLfloat2{405.0f, 411.0f}, GLfloat2{16.0f, 16.0f});
}

void GameMenu::SetSoundState(bool on) { SetTick(16, on); }
void GameMenu::SetHelpState(bool on) { SetTick(17, on); }
void GameMenu::SetClockState(bool on) { SetTick(18, on); }
void GameMenu::SetDimmingState(bool on) { SetTick(19, on); }
void GameMenu::SetSmoothState(bool on) { SetTick(20, on); }
