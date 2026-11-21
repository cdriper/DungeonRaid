#include "intro_screen.h"

#include "dr_gl.h"
#include "font.h"
#include "game.h"
#include "game_tables.h"
#include "rand.h"
#include "util.h"

IntroScreen::IntroScreen(Game* game) : m_game(game), m_begin(&game->m_fonts[0]) {
    m_shade.SetSize(GLfloat2{2.0f, 2.0f});
    m_shade.m_position = GLfloat2{0.5f, 0.5f};
}

// The text wrapped at 0.95 of the screen (7.9166665 font units at scale 0.12), each line centred
// by the width measured up to its break; "Begin" (yellow) below.
void IntroScreen::Initialise() {
    const Font& font = m_game->m_fonts[0];
    m_shade.SetColour(GLcolour4{0, 0, 0, 0});
    m_begin.m_colour = GLcolour4{0xff, 0xff, 0, 0};
    m_begin.UpdateColour();
    m_begin.m_scale = GLfloat2{0.13f, 0.13f};
    m_begin.SetText("Begin", -1);
    m_closing = false;
    m_time = 0.0f;
    m_active = true;
    m_lastTouchState = TOUCHING;
    m_begin.m_position.x = (1.0f - m_begin.m_width * m_begin.m_scale.x) * 0.5f;
    m_begin.m_position.y = 1.43f;
    int lineHeight = font.GetLineHeight();
    float ratio = static_cast<float>(lineHeight - font.GetBase()) / static_cast<float>(lineHeight);
    float line = (ratio * 0.12f) * 1.1f;
    const char* text = g_introText[RandI(0, 0x3c, RAND_COSMETIC)];
    m_lineCount = 0;
    int lineStart = 0;
    int lastBreak = 0;
    float w = 0.0f;           // since the line start
    float breakWidth = 0.0f;  // w at the last break
    int i = 0;
    for (;;) {
        char c = text[i];
        int end;
        bool emit;
        if (c == ' ' || c != '\0') {
            if (c == ' ') {
                breakWidth = w;
                lastBreak = i;
            }
            w = w + (font.GetSize(c).x * 0.5f + font.GetAdvance(text[i], 2.0f));
            if (w > 7.9166665f) {   // 0x40fd5555
                end = lastBreak;
                emit = true;
            } else {
                end = i;
                emit = false;
            }
        } else {
            breakWidth = w;
            end = i;
            emit = true;
        }
        if (emit) {
            if (m_lineCount == static_cast<int>(m_lines.size())) {
                Label l(&font);
                m_lines.push_back(l);
                Label& n = m_lines[m_lineCount];
                n.m_spacing = 2.0f;
                n.m_scale = GLfloat2{0.12f, 0.12f};
            }
            Label& l = m_lines[m_lineCount];
            l.m_position.x = (1.0f + breakWidth * -0.12f) * 0.5f;
            l.m_position.y = 0.22f + static_cast<float>(m_lineCount + 1) * line;
            l.SetText(text + lineStart, end - lineStart);
            lineStart = end + 1;
            ++m_lineCount;
            lastBreak = end;
            w = 0.0f;
        }
        if (text[end] == '\0') break;
        i = end + 1;
    }
    for (int k = 0; k < m_lineCount; ++k) {
        m_lines[k].m_colour.a = 0;
        m_lines[k].UpdateColour();
    }
}

void IntroScreen::FadeOut() {
    m_closing = true;
    m_time = 0.0f;
    m_active = false;
}

// Fades in over a second; then a tap on "Begin" fades it out, saves the game and closes.
void IntroScreen::Process(float dt) {
    bool fadingIn = !m_closing && m_time < 1.0f;
    Game* g = m_game;
    if (m_time == 1.0f) {
        if (g->m_touchState == TOUCHING && m_lastTouchState == NOT_TOUCHING && g->m_touchY > 1.32999992f &&
            g->m_touchX > 0.25f && g->m_touchX < 0.75f) {
            m_begin.m_colour = GLcolour4{0xb4, 0xb4, 0, 0xff};
            m_begin.UpdateColour();
            m_closing = true;
            m_time = 0.0f;
            g->QueueSound(SOUND_START);
        }
    }
    m_lastTouchState = g->m_touchState;
    if (!m_closing && !fadingIn) return;
    float t = dt + m_time;
    m_time = t;
    if (t >= 1.0f) {
        m_time = 1.0f;
        t = 1.0f;
        if (m_closing) {
            m_closing = false;
            m_active = false;
            g->SerialiseState();
            return;
        }
    }
    float alphaF = fadingIn ? Interpolate(255.0f, 0.0f, t) : Interpolate(0.0f, 255.0f, t);
    uint8_t a = static_cast<uint8_t>(FloatToIntArm(alphaF));
    m_shade.SetAlpha(a);
    m_begin.m_colour.a = a;
    m_begin.UpdateColour();
    for (int k = 0; k < m_lineCount; ++k) {
        m_lines[k].m_colour.a = a;
        m_lines[k].UpdateColour();
    }
}

void IntroScreen::Render() {
    sdlDisable(GL_TEXTURE_2D);
    m_shade.Render();
    sdlEnable(GL_TEXTURE_2D);
    for (int k = 0; k < m_lineCount; ++k) m_lines[k].Render();
    m_begin.Render();
}
