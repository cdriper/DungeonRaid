#include "unlock_menu.h"

#include <cstdio>

#include "character_class.h"
#include "dr_gl.h"
#include "font.h"
#include "game.h"
#include "upgrade.h"
#include "util.h"

namespace {
// The static text buffers of the original (0x000899cc title, 0x00089a4c text)
char s_title[128];
char s_text[128];
}  // namespace

UnlockMenu::UnlockMenu(Game* game) : Menu(kQuadCount), m_game(game), m_font(&game->m_fonts[0]), m_title(&game->m_fonts[0]) {
    for (int q = 0; q < kQuadCount; ++q) SetQuadColour(q, 0xff, 0xff, 0xff, 0);
    SetQuad(0, {498.0f, 1010.0f}, {13.0f, 13.0f});
    SetQuad(1, {479.0f, 984.0f}, {1.0f, 13.0f});
    SetQuad(2, {380.0f, 927.0f}, {15.0f, 13.0f});
    SetQuad(3, {478.0f, 999.0f}, {13.0f, 1.0f});
    SetQuad(4, {621.0f, 633.0f}, {1.0f, 1.0f});
    SetQuad(5, {380.0f, 942.0f}, {15.0f, 1.0f});
    SetQuad(6, {498.0f, 994.0f}, {13.0f, 15.0f});
    SetQuad(7, {483.0f, 982.0f}, {1.0f, 15.0f});
    SetQuad(8, {380.0f, 945.0f}, {15.0f, 13.0f});
    m_title.m_colour = GLcolour4{0xff, 0xff, 0, 0};
    m_title.m_scale = GLfloat2{0.12f, 0.12f};
    m_title.UpdateColour();
    m_title.m_spacing = 2.0f;
    m_icon.SetAlpha(0);
}

// Title (yellow), the icon below it, the text lines below that; centred on x 0.5 and y 0.75.
void UnlockMenu::Show(const char* title, const char* text) {
    m_game->QueueSound(SOUND_LEVELUP);
    m_lastTouchState = TOUCHING;
    int lineHeight = m_font->GetLineHeight();
    float ratio = static_cast<float>(lineHeight - m_font->GetBase()) / static_cast<float>(lineHeight);
    float titleLine = ratio * 0.12f;         // 0x3df5c28f
    float line = ratio * 0.120000005f;       // 0x3df5c290
    float half = line * 0.5f;
    m_title.SetText(title, -1);
    InitialiseLabels(text);
    GLfloat2 tl, br;
    Upgrade::GetIconUVs(0, 3, 0x400, 0x200, tl, br);
    float icon = ((br.y - tl.y) * 1024.0f) / 320.0f;
    float labelX = half + 0.040625f;
    float titleY = titleLine + 0.040625f;
    float y0 = (titleY + line) + icon;
    float widest = m_title.m_width * m_title.m_scale.x;
    for (int i = 0; i < m_labelCount; ++i) {
        Label& l = m_labels[i];
        l.m_position.x = labelX;
        l.m_position.y = y0 + static_cast<float>(i) * line;
        float w = l.m_width * l.m_scale.x;
        if (w > widest) widest = w;
    }
    float w = (line + 0.0875f) + widest;
    float centre = w * 0.5f;
    m_title.m_position.y = titleY;
    m_title.m_position.x = centre + (m_title.m_width * m_title.m_scale.x) * -0.5f;
    m_icon.SetUV(tl, br);
    m_icon.m_scale = GLfloat2{icon, icon};
    m_icon.m_position.x = centre;
    m_icon.m_position.y = ((y0 + titleY) + line * -0.5f) * 0.5f;
    float h = ((half + static_cast<float>(m_labelCount) * line) + titleLine) + icon;
    h = h + 0.0875f;
    float x = 0.5f + w * -0.5f;
    m_position.x = x;
    m_position.y = 0.75f + h * -0.5f;
    if (w + x > 1.0f) {
        x = 1.0f - w;
        m_position.x = x;
    }
    m_position.x = static_cast<float>(FloatToIntArm(x * 320.0f)) / 320.0f;
    SetSize(w, h);
    m_active = true;
    m_hiding = false;
}

void UnlockMenu::Show(int classType, int level) {
    ClassType type = static_cast<ClassType>(classType);
    m_promo = false;
    if (level < 2) {
        std::snprintf(s_title, sizeof(s_title), "Class unlocked: %s!", CharacterClass::GetClassName(type));
        Show(s_title, CharacterClass::GetUnlockDescription(type));
    } else {
        std::snprintf(s_title, sizeof(s_title), "%s level up!", CharacterClass::GetClassName(type));
        std::snprintf(s_text, sizeof(s_text),
                      "The %s class has been upgraded to level %d! New customisation options unlocked.",
                      CharacterClass::GetClassName(type), level);
        Show(s_title, s_text);
    }
    GLfloat2 tl, br;
    CharacterClass::GetIconCoords(type, tl, br);
    m_icon.SetUV(GLfloat2{tl.x / 512.0f, tl.y / 512.0f}, GLfloat2{br.x / 512.0f, br.y / 512.0f});
    m_iconTexture = 4;
}

void UnlockMenu::ShowPromo(const char* title, const char* text) {
    m_promo = true;
    Show(title, text);
    m_iconTexture = 1;
}

void UnlockMenu::Hide() {
    if (m_active) m_hiding = true;
}

void UnlockMenu::HideInstantly() {
    m_active = false;
    m_hiding = false;
    for (int q = 0; q < kQuadCount; ++q) SetQuadColour(q, 0xff, 0xff, 0xff, 0);
    m_title.m_colour.a = 0;
    m_title.UpdateColour();
    for (int i = 0; i < m_labelCount; ++i) {
        m_labels[i].m_colour.a = 0;
        m_labels[i].UpdateColour();
    }
    m_icon.SetAlpha(0);
}

// Fades in (8 x 255 per second) or out; when fully shown, a new press starts fading it out.
void UnlockMenu::Process(float dt) {
    if (!m_active) return;
    float step = dt * 255.0f;
    int alpha;
    if (!m_hiding) {
        float a = static_cast<float>(static_cast<int>(m_vertices[0].colour.a)) + step * 8.0f;
        alpha = ClampI(FloatToIntArm(a), 0, 0xff);
    } else {
        float a = static_cast<float>(static_cast<unsigned>(m_vertices[0].colour.a)) + step * -8.0f;
        alpha = FloatToIntArm(a);
    }
    if (alpha < 1) {
        m_hiding = false;
        m_active = false;
        // (a promo would call Game::BonusPromptClosed here: an empty function)
        return;
    }
    uint8_t a8 = static_cast<uint8_t>(alpha);
    for (int v = 0; v < kQuadCount * 4; ++v) m_vertices[v].colour.a = a8;
    m_title.m_colour.a = a8;
    m_title.UpdateColour();
    for (int i = 0; i < m_labelCount; ++i) {
        m_labels[i].m_colour.a = a8;
        m_labels[i].UpdateColour();
    }
    m_icon.SetAlpha(a8);
    if (alpha == 0xff) {
        int touch = m_game->m_touchState;
        if (touch == TOUCHING && m_lastTouchState == NOT_TOUCHING) {
            m_hiding = true;
            m_lastTouchState = m_game->m_touchState;
        } else {
            m_lastTouchState = touch;
        }
    }
}

void UnlockMenu::Render() {
    sdlPushMatrix();
    sdlTranslatef(m_position.x, m_position.y, 0.0f);
    sdlBindTexture(GL_TEXTURE_2D, 1);
    sdlVertexPointer(2, GL_FLOAT, sizeof(QuadVertex), &m_vertices[0].position);
    sdlColorPointer(4, GL_UNSIGNED_BYTE, sizeof(QuadVertex), &m_vertices[0].colour);
    sdlTexCoordPointer(2, GL_FLOAT, sizeof(QuadVertex), &m_vertices[0].uv);
    for (int i = 0; i < kQuadCount * 4; i += 4) sdlDrawArrays(GL_TRIANGLE_FAN, i, 4);
    sdlBindTexture(GL_TEXTURE_2D, m_iconTexture);
    m_icon.Render();
    m_title.Render();
    for (int i = 0; i < m_labelCount; ++i) m_labels[i].Render();
    sdlPopMatrix();
}

// The frame: 13-pixel corners/edges on the left and top, 15 on the right and bottom.
void UnlockMenu::SetSize(float w, float h) {
    int iw = FloatToIntArm((w * 320.0f - 13.0f) - 15.0f);
    int ih = FloatToIntArm((h * 320.0f - 13.0f) - 15.0f);
    float fw = static_cast<float>(iw);
    float right = static_cast<float>(iw + 13);
    float fh = static_cast<float>(ih);
    float bottom = static_cast<float>(ih + 11);
    SetQuad(0, 0.0f, 0.0f, 13.0f, 13.0f);
    SetQuad(1, 13.0f, 0.0f, fw, 13.0f);
    SetQuad(2, right, 0.0f, 15.0f, 13.0f);
    SetQuad(3, 0.0f, 12.0f, 13.0f, fh);
    SetQuad(4, 13.0f, 12.0f, fw, fh);
    SetQuad(5, right, 12.0f, 15.0f, fh);
    SetQuad(6, 0.0f, bottom, 13.0f, 15.0f);
    SetQuad(7, 13.0f, bottom, fw, 15.0f);
    SetQuad(8, right, bottom, 15.0f, 13.0f);
}

// Word wrap as HelpMenu's, at 6 font units (scale 0.1, 0.6 of the screen).
void UnlockMenu::InitialiseLabels(const char* text) {
    m_labelCount = 0;
    int lineStart = 0;
    int lastBreak = 0;
    float w = 0.0f;
    int i = 0;
    for (;;) {
        char c = text[i];
        int end;   // where a line ends, or i
        bool emit;
        if (c == ' ' || c != '\0') {
            int brk = c == ' ' ? i : lastBreak;
            w = w + (m_font->GetSize(c).x * 0.5f + m_font->GetAdvance(text[i], 2.0f));
            if (w > 6.0f) {
                end = brk;
                emit = true;
            } else {
                end = i;
                lastBreak = brk;
                emit = false;
            }
        } else {
            end = i;
            emit = true;
        }
        if (emit) {
            if (m_labelCount == static_cast<int>(m_labels.size())) {
                Label l(m_font);
                m_labels.push_back(l);
                Label& n = m_labels[m_labelCount];
                n.m_spacing = 2.0f;
                n.m_scale = GLfloat2{0.1f, 0.1f};
                n.m_colour.a = 0;
                n.UpdateColour();
            }
            m_labels[m_labelCount].SetText(text + lineStart, end - lineStart);
            lineStart = end + 1;
            ++m_labelCount;
            lastBreak = end;
            w = 0.0f;
        }
        if (text[end] == '\0') return;
        i = end + 1;
    }
}
