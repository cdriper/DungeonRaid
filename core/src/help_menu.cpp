#include "help_menu.h"

#include "dr_gl.h"
#include "font.h"
#include "game.h"
#include "game_state.h"
#include "game_tables.h"
#include "util.h"

HelpMenu::HelpMenu(Game* game) : Menu(kQuadCount), m_game(game) {
    for (int q = 0; q < kQuadCount; ++q) SetQuadColour(q, 0xff, 0xff, 0xff, 0xf0);
    SetQuad(0, {495.0f, 249.0f}, {16.0f, 15.0f});
    SetQuad(1, {399.0f, 391.0f}, {1.0f, 15.0f});
    SetQuad(2, {380.0f, 391.0f}, {17.0f, 15.0f});
    SetQuad(3, {380.0f, 408.0f}, {16.0f, 1.0f});
    SetQuad(4, {383.0f, 402.0f}, {1.0f, 1.0f});
    SetQuad(5, {478.0f, 429.0f}, {17.0f, 1.0f});
    SetQuad(6, {426.0f, 269.0f}, {16.0f, 17.0f});
    SetQuad(7, {399.0f, 407.0f}, {1.0f, 17.0f});
    SetQuad(8, {408.0f, 269.0f}, {17.0f, 17.0f});
    SetQuad(9, {399.0f, 391.0f}, {1.0f, 15.0f});
    SetQuad(10, {399.0f, 407.0f}, {1.0f, 17.0f});
    // (quads 11 and 12 are left uninitialised by the original: zero here)
}

void HelpMenu::Initialise(float x, float y, float w, float h, int help) {
    if (m_active || ((m_shownHelp >> (help & 0xff)) & 1) == 0) return;
    m_shownHelp ^= 1 << (help & 0xff);
    if (m_shownHelp == 0) m_game->SetHelpState(false);
    Show(g_helpText[help], x, y, w, h);
}

// The bubble: 0.98 wide, centred; below the point (y < 0.75, by h) or above it (by w: the
// original's choice), its pointer at x.
void HelpMenu::Show(const char* text, float x, float y, float w, float h) {
    m_fadeTime = 0.2f;
    m_closing = false;
    m_active = true;
    m_width = 0.98f;
    m_lastTouchState = TOUCHING;
    InitialiseLabels(text, 0.88f);
    for (int v = 0; v < kQuadCount * 4; ++v) m_vertices[v].colour.a = 0;
    for (int i = 0; i < m_labelCount; ++i) {
        m_labels[i].m_colour.a = 0;
        m_labels[i].UpdateColour();
    }
    bool below = y < 0.75f;
    float offset = below ? h : -w;
    float top = offset + y;
    float pointer = Clamp(x, 0.1f, 0.9f);
    m_height = (0.1f + static_cast<float>(m_labelCount) * 0.05f) + 0.03f;
    for (int i = 0; i < m_labelCount; ++i) {
        m_labels[i].m_position.x = 0.06f;
        m_labels[i].m_position.y = 0.1f + static_cast<float>(i) * 0.05f;
    }
    float left = (1.0f - m_width) * 0.5f;
    m_position.x = left;
    float gap = below ? 0.0625f : -0.0625f - m_height;
    float px = static_cast<float>(FloatToIntArm(left * 320.0f)) / 320.0f;
    float py = static_cast<float>(FloatToIntArm((gap + top) * 320.0f)) / 320.0f;
    m_position.x = px;
    m_position.y = py;
    SetSize(m_width, m_height, (pointer - px) / m_width, below);
}

void HelpMenu::QueueHelp(const char* text, float x, float y, float w) {
    if (!m_active) {
        Show(text, x, y, w, w);
    } else {
        m_queue.push_back(QueuedHelp{text, x, y, w});
    }
}

// Fading in/out (0.2 s, alpha 240); then a press on the bubble fades it out; once faded out, the
// next queued text shows.
void HelpMenu::Process(float dt) {
    if (m_fadeTime <= 0.0f) {
        Game* g = m_game;
        int touch = g->m_touchState;
        if (touch == TOUCHING && m_lastTouchState == NOT_TOUCHING) {
            float tx = g->m_touchX;
            float ty = g->m_touchY;
            if (m_position.x < tx && tx < m_position.x + m_width && m_position.y < ty && ty < m_position.y + m_height) {
                FadeOut();
                touch = g->m_touchState;
            }
        }
        m_lastTouchState = touch;
        return;
    }
    float left = m_fadeTime - dt;
    m_fadeTime = left;
    float from = 0.0f;
    float to = 240.0f;
    if (left > 0.0f) {
        if (m_closing) {
            from = 240.0f;
            to = 0.0f;
        }
    } else {
        m_fadeTime = 0.0f;
        left = 0.0f;
        if (m_closing) {
            m_active = false;
            if (m_queue.empty()) return;
            QueuedHelp& next = m_queue.front();
            Show(next.text.c_str(), next.x, next.y, next.w, next.w);
            m_queue.pop_front();
            return;
        }
    }
    uint8_t alpha = static_cast<uint8_t>(FloatToIntArm(Interpolate(from, to, left / 0.2f)));
    for (int v = 0; v < kQuadCount * 4; ++v) m_vertices[v].colour.a = alpha;
    for (int i = 0; i < m_labelCount; ++i) {
        m_labels[i].m_colour.a = alpha;
        m_labels[i].UpdateColour();
    }
}

void HelpMenu::Render() {
    sdlPushMatrix();
    sdlTranslatef(m_position.x, m_position.y, 0.0f);
    sdlBindTexture(GL_TEXTURE_2D, 1);
    sdlVertexPointer(2, GL_FLOAT, sizeof(QuadVertex), &m_vertices[0].position);
    sdlColorPointer(4, GL_UNSIGNED_BYTE, sizeof(QuadVertex), &m_vertices[0].colour);
    sdlTexCoordPointer(2, GL_FLOAT, sizeof(QuadVertex), &m_vertices[0].uv);
    for (int i = 0; i < kQuadCount * 4; i += 4) sdlDrawArrays(GL_TRIANGLE_FAN, i, 4);
    for (int i = 0; i < m_labelCount; ++i) m_labels[i].Render();
    sdlPopMatrix();
}

// Word wrap: glyph widths (half the glyph size plus the advance) summed in font units until they
// pass width / 0.1; the line breaks at the last space. Black text at scale 0.1.
void HelpMenu::InitialiseLabels(const char* text, float width) {
    const Font& font = m_game->m_fonts[0];
    float limit = width / 0.1f;
    m_labelCount = 0;
    int lineStart = 0;
    int lastBreak = 0;
    float w = 0.0f;
    for (int i = 0;; ++i) {
        char c = text[i];
        if (c == ' ' || c != '\0') {
            if (c == ' ') lastBreak = i;
            w = w + (font.GetSize(c).x * 0.5f + font.GetAdvance(text[i], 2.0f));
        } else {
            w = 1000.0f;
            lastBreak = i;
        }
        int at = i;
        if (w > limit) {
            if (m_labelCount == static_cast<int>(m_labels.size())) {
                Label l(&font);
                m_labels.push_back(l);
                Label& n = m_labels[m_labelCount];
                n.m_spacing = 2.0f;
                n.m_scale = GLfloat2{0.1f, 0.1f};
                n.m_colour = GLcolour4{0, 0, 0, 0};
                n.UpdateColour();
            }
            m_labels[m_labelCount].SetText(text + lineStart, lastBreak - lineStart);
            lineStart = lastBreak + 1;
            ++m_labelCount;
            i = lastBreak;
            at = lastBreak;
            w = 0.0f;
        }
        if (text[at] == '\0') return;
    }
}

// The frame in whole pixels: w x h, the pointer `pointer` of the way along (its notch 6 or 20
// pixels left of it), on the top edge when below.
void HelpMenu::SetSize(float w, float h, float pointer, bool below) {
    bool small = pointer < 0.5f;
    int iw = FloatToIntArm((w * 320.0f - 16.0f) - 17.0f);
    int ih = FloatToIntArm((h * 320.0f - 15.0f) - 17.0f);
    int right = iw + 16;
    int bottom = ih + 15;
    int arrow = FloatToIntArm(w * 320.0f * pointer) - (small ? 6 : 20);
    int beforeArrow = arrow - 16;
    float fw = static_cast<float>(iw);
    float fh = static_cast<float>(ih);
    float fRight = static_cast<float>(right);
    float fBottom = static_cast<float>(bottom);
    SetQuad(0, 0.0f, 0.0f, 16.0f, 15.0f);
    SetQuad(1, 16.0f, 0.0f, below ? static_cast<float>(beforeArrow) : fw, 15.0f);
    SetQuad(2, fRight, 0.0f, 17.0f, 15.0f);
    SetQuad(3, 0.0f, 15.0f, 16.0f, fh);
    SetQuad(4, 16.0f, 15.0f, fw, fh);
    SetQuad(5, fRight, 15.0f, 17.0f, fh);
    SetQuad(6, 0.0f, fBottom, 16.0f, 17.0f);
    SetQuad(7, 16.0f, fBottom, below ? fw : static_cast<float>(beforeArrow), 17.0f);
    SetQuad(8, fRight, fBottom, 17.0f, 17.0f);
    float fArrow = static_cast<float>(arrow);
    float rest = static_cast<float>(right - arrow);
    const GLfloat2 parked{1000.0f, 1000.0f};
    const GLfloat2 zero{0.0f, 0.0f};
    if (below) {   // pointer up, on the top edge
        SetQuad(11, {small ? 454.0f : 483.0f, 265.0f}, {28.0f, 39.0f});
        SetQuad(11, fArrow, -24.0f, 28.0f, 39.0f);
        SetQuad(9, {399.0f, 391.0f}, {1.0f, 15.0f});
        SetQuad(9, fArrow + 28.0f, 0.0f, rest - 28.0f, 15.0f);
        SetQuad(10, parked, zero, zero, 1.0f, 1.0f);
        SetQuad(12, parked, zero, zero, 1.0f, 1.0f);
    } else {       // pointer down, on the bottom edge
        SetQuad(12, {small ? 454.0f : 483.0f, 305.0f}, {28.0f, 41.0f});
        SetQuad(12, fArrow, fBottom, 28.0f, 41.0f);
        SetQuad(10, {399.0f, 407.0f}, {1.0f, 17.0f});
        SetQuad(10, fArrow + 28.0f, fBottom, rest - 28.0f, 17.0f);
        SetQuad(9, parked, zero, zero, 1.0f, 1.0f);
        SetQuad(11, parked, zero, zero, 1.0f, 1.0f);
    }
}

void HelpMenu::FadeOut() {
    m_closing = true;
    m_fadeTime = 0.2f;
}

void HelpMenu::ResetHelp() {
    for (int i = 0; i < kPageCount; ++i) m_shownHelp |= 1 << i;
}

void HelpMenu::ClearHelp() { m_shownHelp = 0; }

void HelpMenu::Serialise(GameState* state) { state->WriteInteger(m_shownHelp); }

void HelpMenu::Deserialise(GameState* state) { m_shownHelp = state->ReadInteger(); }
