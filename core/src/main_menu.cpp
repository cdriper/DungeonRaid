#include "main_menu.h"

#include <cstring>

#include "dr_gl.h"
#include "game.h"
#include "util.h"
#include "version.h"

namespace {
float F(uint32_t bits) {   // a float given by its bits (the constants the original stores as words)
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}
const float kPx = 1.0f / 1024.0f;   // texture 1 pixels
const float kPx4 = 1.0f / 512.0f;   // texture 4 pixels

// SetUV, then the position and scale words the original stores directly
void Place(Quad& q, float u0, float v0, float u1, float v1, uint32_t x, uint32_t y, uint32_t sx, uint32_t sy) {
    q.SetUV(GLfloat2{u0 * kPx, v0 * kPx}, GLfloat2{u1 * kPx, v1 * kPx});
    q.m_position = GLfloat2{F(x), F(y)};
    q.m_scale = GLfloat2{F(sx), F(sy)};
}
}  // namespace

MainMenu::MainMenu(Game* game) : m_game(game), m_versionLabel(&game->m_fonts[0]) {
    m_versionLabel.m_scale = GLfloat2{0.075f, 0.075f};
    m_versionLabel.m_colour = GLcolour4{0xff, 0xe8, 0xb0, 0xff};
    m_background.SetUV(GLfloat2{0.5f, 0.5f}, GLfloat2{0.8125f, 0.96875f});
    m_background.m_scale = GLfloat2{1.0f, 1.5f};
    m_logo.m_position = GLfloat2{F(0x3f4a6666), F(0x3fba6666)};
    m_logo.SetUV(GLfloat2{0.5f, 0.97265625f}, GLfloat2{646 * kPx, 1.0f});
    m_logo.m_scale = GLfloat2{F(0x3ed66666), F(0x3db33333)};
    m_logo.SetAlpha(0xff);
    m_buttons[0].m_position = GLfloat2{F(0x3e54cccd), F(0x3eb4cccd)};
    m_buttons[0].SetUV(GLfloat2{833 * kPx, 513 * kPx}, GLfloat2{964 * kPx, 552 * kPx});
    m_buttons[0].m_scale = GLfloat2{F(0x3ed1999a), F(0x3df9999a)};
    m_buttons[0].SetAlpha(0);
    m_buttons[1].m_position = GLfloat2{F(0x3e69999a), F(0x3f066666)};
    m_buttons[1].SetUV(GLfloat2{833 * kPx, 553 * kPx}, GLfloat2{977 * kPx, 592 * kPx});
    m_buttons[1].m_scale = GLfloat2{F(0x3ee66666), F(0x3df9999a)};
    m_buttons[1].SetAlpha(0);
    m_buttons[2].m_position = GLfloat2{F(0x3e666666), F(0x3f1e6666)};
    m_buttons[2].SetUV(GLfloat2{881 * kPx, 472 * kPx}, GLfloat2{1023 * kPx, 511 * kPx});
    m_buttons[2].m_scale = GLfloat2{F(0x3ee33333), F(0x3df9999a)};
    m_buttons[2].SetAlpha(0);
    m_buttons[3].m_position = GLfloat2{F(0x3e500000), F(0x3f366666)};
    m_buttons[3].SetUV(GLfloat2{833 * kPx, 593 * kPx}, GLfloat2{961 * kPx, 633 * kPx});
    m_buttons[3].m_scale = GLfloat2{F(0x3ecccccd), 0.125f};
    m_buttons[3].SetAlpha(0);
    m_buttons[4].m_position = GLfloat2{F(0x3e1e6666), F(0x3f5d3333)};
    m_buttons[4].SetUV(GLfloat2{833 * kPx, 634 * kPx}, GLfloat2{930 * kPx, 675 * kPx});
    m_buttons[4].m_scale = GLfloat2{F(0x3e9b3333), F(0x3e033333)};
    m_buttons[4].SetAlpha(0);
    m_buttons[6].m_position = GLfloat2{F(0x3e69999a), F(0x3f73999a)};
    m_buttons[6].SetUV(GLfloat2{1 * kPx, 984 * kPx}, GLfloat2{145 * kPx, 1023 * kPx});
    m_buttons[6].m_scale = GLfloat2{F(0x3ee66666), F(0x3df9999a)};
    m_buttons[6].SetAlpha(0);
    m_buttons[7].m_position = GLfloat2{F(0x3e866666), F(0x3f8b6666)};
    m_buttons[7].SetUV(GLfloat2{147 * kPx, 983 * kPx}, GLfloat2{313 * kPx, 1023 * kPx});
    m_buttons[7].m_scale = GLfloat2{F(0x3f04cccd), 0.125f};
    m_buttons[7].SetAlpha(0);
    m_buttons[5].SetAlpha(0);
    m_buttons[5].m_rotation = -90.0f;
    // port: "Achievements" (texture 1 at 824,380, 192 x 40; drawn in the buttons' font and style,
    // 11 pixels before the first letter as the original buttons), left edge in line with the
    // others, where the OpenFeint button was
    m_achievementsButton.SetUV(GLfloat2{824 * kPx, 380 * kPx}, GLfloat2{1016 * kPx, 420 * kPx});
    m_achievementsButton.m_scale = GLfloat2{192.0f / 320.0f, 40.0f / 320.0f};
    m_achievementsButton.m_position = GLfloat2{0.003f + m_achievementsButton.m_scale.x * 0.5f, F(0x3f73999a)};
    m_achievementsButton.SetAlpha(0);
    // port: "Settings", made from the other buttons' letters (texture 1 at 912,200, 107 x 42), a row
    // under Achievements
    m_settingsButton.SetUV(GLfloat2{912 * kPx, 200 * kPx}, GLfloat2{1019 * kPx, 242 * kPx});
    m_settingsButton.m_scale = GLfloat2{107.0f / 320.0f, 42.0f / 320.0f};
    // (the image has 4 pixels before its first letter, the original buttons 11: 7 more to the right)
    m_settingsButton.m_position = GLfloat2{0.003f + 7.0f / 320.0f + m_settingsButton.m_scale.x * 0.5f, F(0x3f73999a) + 0.0875f};
    m_settingsButton.SetAlpha(0);
    // port: "Exit" (texture 1 at 730,466, 77 x 40; as Achievements), apart below, where the
    // (never shown) Bonus Item button was
    m_exitButton.SetUV(GLfloat2{730 * kPx, 466 * kPx}, GLfloat2{807 * kPx, 506 * kPx});
    m_exitButton.m_scale = GLfloat2{77.0f / 320.0f, 40.0f / 320.0f};
    m_exitButton.m_position = GLfloat2{0.003f + m_exitButton.m_scale.x * 0.5f, 1.19f};
    m_exitButton.SetAlpha(0);
    m_confirmShade.SetSize(GLfloat2{2.0f, 2.0f});
    m_confirmShade.m_position = GLfloat2{0.5f, 0.5f};
    m_confirmShade.SetColour(GLcolour4{0, 0, 0, 0});
    m_confirmPanel.m_position = GLfloat2{0.5f, 0.5f};
    m_confirmPanel.SetUV(GLfloat2{1 * kPx4, 355 * kPx4}, GLfloat2{247 * kPx4, 511 * kPx4});
    m_confirmPanel.SetSize(GLfloat2{0.76875f, 0.4875f});
    m_confirmPanel.SetAlpha(0);
    m_confirmYes.m_position = GLfloat2{F(0x3f3df3b6), F(0x3f25a1cb)};
    m_confirmYes.SetUV(GLfloat2{1 * kPx4, 313 * kPx4}, GLfloat2{50 * kPx4, 335 * kPx4});
    m_confirmYes.SetSize(GLfloat2{0.153125f, 0.06875f});
    m_confirmYes.SetColour(GLcolour4{100, 100, 100, 0xff});
    m_confirmNo.m_position = GLfloat2{F(0x3e7ced91), F(0x3f25a1cb)};
    m_confirmNo.SetUV(GLfloat2{1 * kPx4, 336 * kPx4}, GLfloat2{41 * kPx4, 354 * kPx4});
    m_confirmNo.SetSize(GLfloat2{0.125f, 0.05625f});
    m_confirmNo.SetColour(GLcolour4{100, 100, 100, 0xff});
}

void MainMenu::Show() {
    m_canContinue = m_game->m_saveState.GetStateSize() > 0;
    m_hasScores = m_game->GetScoreCount() > 0;
    m_background.SetAlpha(0xff);
    for (Quad& b : m_buttons) b.SetAlpha(0xff);
    SetPortAlphas(0xff);
    if (!m_canContinue) m_buttons[0].SetAlpha(0x80);
    if (!m_hasScores) m_buttons[4].SetAlpha(0x80);
}

void MainMenu::SetPortAlphas(uint8_t alpha) {
    m_achievementsButton.SetAlpha(alpha);
    m_settingsButton.SetAlpha(alpha);
    m_exitButton.SetAlpha(alpha);
}

bool MainMenu::ExitShown() const { return m_game->m_desktop && m_game->m_canQuit; }

// The port button under p. The buttons' glows overlap their neighbours': of the ones containing p
// (Scores too, when it has scores) the one whose centre is nearest in y wins; -1 when none or Scores.
int MainMenu::PortButtonAt(const GLfloat2& p) const {
    struct Candidate {
        const Quad* quad;
        int id;
    };
    const Candidate candidates[] = {
        {&m_buttons[4], m_hasScores ? 4 : -2},
        {&m_achievementsButton, kAchievementsPressed},
        {&m_settingsButton, kSettingsPressed},
        {&m_exitButton, ExitShown() ? kExitPressed : -2},
    };
    int best = -1;
    float bestDistance = 0.0f;
    for (const Candidate& c : candidates) {
        if (c.id == -2 || !c.quad->ContainsPoint(p)) continue;
        const float d = p.y > c.quad->m_position.y ? p.y - c.quad->m_position.y : c.quad->m_position.y - p.y;
        if (best == -1 || d < bestDistance) {
            best = c.id;
            bestDistance = d;
        }
    }
    return best == 4 ? -1 : best;
}

void MainMenu::SetActive(bool active) {
    m_active = active;
    if (active) m_lastTouchState = m_game->m_touchState;
}

void MainMenu::FadeIn() {
    m_fadeTime = 0.5f;
    m_active = true;
    m_fadingIn = true;
    m_background.SetAlpha(0);
    for (Quad& b : m_buttons) b.SetAlpha(0);
    SetPortAlphas(0);
}

void MainMenu::FadeOut() {
    m_fadeTime = 0.3f;
    m_fadingIn = false;
}

void MainMenu::SetSoundState(bool on) {
    Quad& b = m_buttons[5];
    if (!on) {
        b.m_position = GLfloat2{F(0x3f42cccd), F(0x3fb80000)};
        b.SetUV(GLfloat2{983 * kPx, 513 * kPx}, GLfloat2{1023 * kPx, 660 * kPx});
        b.m_scale = GLfloat2{F(0x3eeb3333), F(0x3e000000)};
    } else {
        b.m_position = GLfloat2{F(0x3f460000), F(0x3fb80000)};
        b.SetUV(GLfloat2{923 * kPx, 676 * kPx}, GLfloat2{963 * kPx, 815 * kPx});
        b.m_scale = GLfloat2{F(0x3ede6666), F(0x3e000000)};
    }
}

// Buttons 0 (no saved game) and 4 (no scores) at half alpha (alpha / 2, truncated), the others at
// `full` (the low byte of alpha, or its complement while the logo fades).
void MainMenu::SetButtonAlphas(int alpha, uint8_t full) {
    for (int i = 0; i < kButtonCount; ++i) {
        bool dim = (i == 0 && !m_canContinue) || (i == 4 && !m_hasScores);
        m_buttons[i].SetAlpha(dim ? static_cast<uint8_t>(alpha / 2) : full);
    }
    SetPortAlphas(full);
}

// Port: a tap on the build info (bottom right). Three within a second each toggle debug mode.
bool MainMenu::VersionTapped(const GLfloat2& p) {
    const Label& v = m_versionLabel;
    const float w = v.m_width * v.m_scale.x;
    const float h = v.m_scale.y;
    const float margin = 0.02f;
    if (v.m_glyphCount == 0 || p.x < v.m_position.x - margin || p.x > v.m_position.x + w + margin ||
        p.y < v.m_position.y - h - margin || p.y > v.m_position.y + h * 0.3f + margin) {
        return false;
    }
    const double now = GetTime();
    m_versionTaps = (m_versionTaps > 0 && now - m_versionTapTime < 1.0) ? m_versionTaps + 1 : 1;
    m_versionTapTime = now;
    if (m_versionTaps >= 3) {
        m_versionTaps = 0;
        m_game->SetDebugMode(!m_game->m_debugMode);
        m_game->ShowToast(m_game->m_debugMode ? "Debug mode on" : "Debug mode off");
    }
    return true;
}

void MainMenu::StartAction(int action) {
    if (action == 0) {
        m_game->NewGame();
    } else if (action == 1) {
        m_game->StartChallenge();
    } else {
        m_game->StartTutorial();
    }
}

void MainMenu::Process(float dt) {
    Game* g = m_game;
    m_canContinue = g->m_saveState.GetStateSize() > 0;
    m_hasScores = g->GetScoreCount() > 0;
    if (m_fadeTime > 0.0f) {
        float t = m_fadeTime - dt;
        m_fadeTime = t;
        int a;
        if (!m_fadingIn) {
            if (t < 0.0f) {
                m_fadeTime = 0.0f;
                m_active = false;
                t = 0.0f;
            }
            a = FloatToIntArm(Interpolate(255.0f, 0.0f, t / 0.3f));
        } else {
            if (t < 0.0f) {
                m_fadeTime = 0.0f;
                t = 0.0f;
            }
            a = FloatToIntArm(Interpolate(0.0f, 255.0f, t + t));
        }
        m_background.SetAlpha(static_cast<uint8_t>(a));
        SetButtonAlphas(a, static_cast<uint8_t>(a));
        return;
    }
    if (m_confirming) {
        float t = m_confirmTime - dt;
        m_confirmTime = t;
        float from = 0.0f;
        float to = 255.0f;
        if (t < 0.0f) {
            m_confirmTime = 0.0f;
            t = 0.0f;
            if (m_confirmClosing) {
                m_confirmShade.SetAlpha(0);
                m_confirmPanel.SetAlpha(0);
                m_confirmYes.SetAlpha(0);
                m_confirmNo.SetAlpha(0);
                m_confirming = false;
                if (m_confirmYesChosen) StartAction(m_confirmAction);
                return;
            }
        } else if (m_confirmClosing) {
            from = 255.0f;
            to = 0.0f;
        }
        int a = FloatToIntArm(Interpolate(from, to, t / 0.3f));
        m_confirmShade.SetAlpha(static_cast<uint8_t>(a / 2));
        m_confirmPanel.SetAlpha(static_cast<uint8_t>(a));
        m_confirmYes.SetAlpha(static_cast<uint8_t>(a));
        m_confirmNo.SetAlpha(static_cast<uint8_t>(a));
        if (m_confirmTime == 0.0f) {
            int touch = g->m_touchState;
            if (touch == TOUCHING && m_lastTouchState == NOT_TOUCHING) {
                GLfloat2 p{g->m_touchX, g->m_touchY};
                if (m_confirmYes.ContainsPoint(p)) {
                    m_confirmYesChosen = true;
                    m_confirmClosing = true;
                    m_confirmTime = 0.3f;
                    g->QueueSound(SOUND_CLICK);
                } else if (m_confirmNo.ContainsPoint(p)) {
                    m_confirmYesChosen = false;
                    m_confirmTime = 0.3f;
                    m_confirmClosing = true;
                    g->QueueSound(SOUND_CLICK);
                }
                m_lastTouchState = g->m_touchState;
            } else {
                m_lastTouchState = touch;
            }
        }
        return;
    }
    uint8_t logo = m_logo.m_vertices[0].colour.a;
    if (logo != 0) {   // the logo fades out first (500 per second)
        int v = FloatToIntArm(static_cast<float>(static_cast<int>(logo)) + dt * -500.0f);
        v &= ~(v >> 31);
        uint8_t b = static_cast<uint8_t>(v);
        m_logo.SetAlpha(b);
        SetButtonAlphas(0xff - v, static_cast<uint8_t>(~b));
        return;
    }
    int touch = g->m_touchState;
    GLfloat2 p{g->m_touchX, g->m_touchY};
    if (touch == TOUCHING) {
        if (m_lastTouchState == NOT_TOUCHING) {
            if (m_bonusVisible && m_buttons[7].ContainsPoint(p)) {
                g->QueueSound(SOUND_ABILITY_READY);
                m_lastTouchState = g->m_touchState;
                g->ShowBonusRequest();
                return;
            }
            if (VersionTapped(p)) {   // port
                m_lastTouchState = g->m_touchState;
                return;
            }
            // port: Achievements, Settings, Exit
            const int port = PortButtonAt(p);
            if (port >= 0) {
                g->QueueSound(SOUND_CLICK);
                Quad& b = port == kSettingsPressed ? m_settingsButton
                          : port == kExitPressed   ? m_exitButton
                                                   : m_achievementsButton;
                b.SetAlpha(0x80);
                m_pressed = port;
                m_lastTouchState = g->m_touchState;
                return;
            }
            // the buttons in order, the dimmed ones skipped; k counts one ahead of i (two after
            // skipping the scores button), and the scan stops once k passes 7
            int i = 0;
            int k = 1;
            for (;;) {
                if (i == 4 && !m_hasScores) {
                    ++k;
                    i = 5;
                }
                if ((i != 0 || m_canContinue) && i != 5 && i != 6) {   // port: 5 (sound), 6 (OpenFeint) removed
                    if (m_buttons[i].ContainsPoint(p)) {
                        g->QueueSound(SOUND_CLICK);
                        m_buttons[i].SetAlpha(0x80);
                        m_pressed = i;
                        m_lastTouchState = g->m_touchState;
                        return;
                    }
                    if (k > 7) {
                        m_lastTouchState = g->m_touchState;
                        return;
                    }
                }
                ++i;
                ++k;
            }
        }
    } else if (touch == NOT_TOUCHING) {
        for (Quad& b : m_buttons) b.SetAlpha(0xff);
        SetPortAlphas(0xff);
        if (!m_canContinue) m_buttons[0].SetAlpha(0x80);
        if (!m_hasScores) m_buttons[4].SetAlpha(0x80);
        if (m_pressed < 0) {
            m_lastTouchState = g->m_touchState;
            return;
        }
        if (m_pressed >= kButtonCount) {   // port: Settings, Achievements, Exit
            if (m_pressed == kSettingsPressed && m_settingsButton.ContainsPoint(p)) g->m_settingsScreen.Show();
            if (m_pressed == kExitPressed && m_exitButton.ContainsPoint(p)) g->m_quitRequested = true;
            if (m_pressed == kAchievementsPressed && m_achievementsButton.ContainsPoint(p)) g->m_achievementsScreen.Show();
            m_pressed = -1;
            m_lastTouchState = g->m_touchState;
            return;
        }
        if (m_buttons[m_pressed].ContainsPoint(p)) {
            switch (m_pressed) {
                case 0: g->ContinueGame(); break;
                case 1:
                case 2:
                case 3:
                    if (!m_canContinue) {
                        StartAction(m_pressed - 1);
                    } else {   // the saved game would be lost: ask first
                        m_confirming = true;
                        m_confirmTime = 0.3f;
                        m_confirmClosing = false;
                        m_confirmAction = m_pressed - 1;
                    }
                    break;
                case 4: g->ShowScores(); break;
                case 5:
                    g->SetSoundState(!g->m_soundEnabled);
                    if (g->m_soundEnabled) g->QueueSound(SOUND_CLICK);
                    break;
                case 6: g->m_showFeint = true; break;
                default: break;
            }
        }
        m_pressed = -1;
        touch = g->m_touchState;
    }
    m_lastTouchState = touch;
}

void MainMenu::Render() {
    sdlBindTexture(GL_TEXTURE_2D, 1);
    m_background.Render();
    m_logo.Render();
    for (int i = 0; i < kButtonCount; ++i) {
        if (i != 5 && i != 6) m_buttons[i].Render();   // port: 5 (sound), 6 (OpenFeint) not drawn
        if (i == 6 && !m_bonusVisible) break;
    }
    m_achievementsButton.Render();   // port (texture 1 still bound)
    m_settingsButton.Render();
    if (ExitShown()) m_exitButton.Render();
    // port: the version and build time, faded with the background (fonts are loaded by now)
    if (m_versionLabel.m_glyphCount == 0) {   // right-aligned, bottom right (where the sound button was)
        m_versionLabel.SetText(GetBuildInfo());
        m_versionLabel.m_position = GLfloat2{0.97f - m_versionLabel.m_width * m_versionLabel.m_scale.x, 1.455f};
    }
    m_versionLabel.m_colour.a = m_background.m_vertices[0].colour.a;
    m_versionLabel.UpdateColour();
    m_versionLabel.Render();
    if (!m_confirming) return;
    sdlDisable(GL_TEXTURE_2D);
    m_confirmShade.Render();
    sdlEnable(GL_TEXTURE_2D);
    sdlBindTexture(GL_TEXTURE_2D, 4);
    m_confirmPanel.Render();
    if (m_confirmClosing) {
        if (m_confirmYesChosen) {
            m_confirmYes.Render();
        } else {
            m_confirmNo.Render();
        }
    }
}
