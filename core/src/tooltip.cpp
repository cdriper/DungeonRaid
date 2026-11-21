#include "tooltip.h"

#include <cstdio>
#include <string>

#include "character_class.h"
#include "dr_gl.h"
#include "font.h"
#include "game.h"
#include "skill.h"
#include "tile.h"
#include "util.h"

namespace {
const GLcolour4 kWhite{0xff, 0xff, 0xff, 0xff};
}  // namespace

Tooltip::Tooltip(Game* game) : Menu(9), m_game(game), m_font(&game->m_fonts[0]), m_title(&game->m_fonts[0]) {
    for (int q = 0; q < 9; ++q) SetQuadColour(q, 0xff, 0xff, 0xff, 0);
    SetQuad(0, {498.0f, 498.0f}, {13.0f, 13.0f});
    SetQuad(1, {479.0f, 472.0f}, {1.0f, 13.0f});
    SetQuad(2, {380.0f, 415.0f}, {15.0f, 13.0f});
    SetQuad(3, {478.0f, 487.0f}, {13.0f, 1.0f});
    SetQuad(4, {145.0f, 399.0f}, {1.0f, 1.0f});
    SetQuad(5, {380.0f, 430.0f}, {15.0f, 1.0f});
    SetQuad(6, {498.0f, 482.0f}, {13.0f, 15.0f});
    SetQuad(7, {483.0f, 470.0f}, {1.0f, 15.0f});
    SetQuad(8, {380.0f, 433.0f}, {15.0f, 13.0f});
    m_title.m_scale = GLfloat2{0.11f, 0.11f};
    m_title.m_colour = GLcolour4{0xff, 0xff, 0, 0};
    m_title.m_spacing = 2.0f;
    m_title.UpdateColour();
}

// The frame: 13-pixel corners/edges on the left and top, 15 on the right and bottom, the middle
// stretched to w x h (screen units).
void Tooltip::SetSize(float w, float h) {
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

void Tooltip::Show(const GLfloat2& position, int offset) {
    m_visible = true;
    m_hiding = false;
    int lineHeight = m_font->GetLineHeight();
    float line = (static_cast<float>(lineHeight - m_font->GetBase()) / static_cast<float>(lineHeight)) * 0.11f;
    float half = line * 0.5f;
    float x = half + 0.040625f;
    float top = line + 0.040625f;
    m_title.m_position = GLfloat2{x, top};
    float w = m_title.m_width * m_title.m_scale.x;
    for (int i = 0; i < m_labelCount; ++i) {
        Label& l = m_labels[i];
        l.m_position.x = x;
        l.m_position.y = top + line * static_cast<float>(i + 1);
        float lw = l.m_width * l.m_scale.x;
        if (w < lw) w = lw;
    }
    const float pad = 0.0875f;
    w = w + (line + pad);
    float h = ((half + line * static_cast<float>(m_labelCount)) + line) + pad;
    float y = position.y;
    float shift;
    if (y > 1.0f) {   // low on the screen: above the point
        shift = static_cast<float>(offset) / -320.0f;
        y = y - h;
    } else {
        shift = static_cast<float>(offset) / 320.0f;
    }
    m_position.y = y + shift;
    float px = position.x - 0.015625f;
    m_position.x = px;
    if (w + px > 1.0f) px = 1.0f - w;   // keep it on screen
    m_position.x = static_cast<float>(FloatToIntArm(px * 320.0f)) / 320.0f;   // whole pixels
    SetSize(w, h);
}

void Tooltip::Show(EnemyType type, const GLfloat2& position) {
    RaceType race = m_game->m_classes[m_game->m_currentClass].m_race;
    if (type == ENEMY_NORMAL || type != CharacterClass::GetMortalEnemyType(race)) {
        InitialiseLabels(Tile::GetEnemyName(type), Tile::GetEnemyDescription(type), kWhite);
    } else {
        std::string title = std::string(Tile::GetEnemyName(type)) + " (Mortal Enemy)";   // static buffer 0x00089978
        InitialiseLabels(title.c_str(), Tile::GetEnemyDescription(type), kWhite);
        InitialiseLabels(nullptr, CharacterClass::GetMortalEnemyDescription(race), GLcolour4{0xff, 0x46, 0x3c, 0xff});
        m_title.m_colour = GLcolour4{0xff, 0x46, 0x3c, 0xff};
        m_title.UpdateColour();
    }
    Show(position, 0x19);
}

void Tooltip::Show(SkillType type, int cooldown, int charge, const GLfloat2& position) {
    InitialiseLabels(Skill::GetSkillName(type), Skill::GetSkillDescription(type), kWhite);
    if (cooldown > 0) {
        char text[64];   // static buffer 0x000899b8 in the original
        std::snprintf(text, sizeof(text), "Cooldown: %d/%d", charge, cooldown);
        AddLabel(text, kWhite);
    }
    Show(position, 0);
}

void Tooltip::ShowScroll(SkillType type, const GLfloat2& position) {
    std::string title = std::string("Scroll of ") + Skill::GetSkillName(type);   // 0x000897f8
    std::string text = std::string("Instantly activates ") + Skill::GetSkillName(type) + ": " +
                       Skill::GetSkillDescription(type);   // 0x00089878
    InitialiseLabels(title.c_str(), text.c_str(), kWhite);
    Show(position, 0x19);
}

// A title (yellow; null keeps the current one and appends the lines) and the text split into lines.
void Tooltip::InitialiseLabels(const char* title, const char* text, GLcolour4 colour) {
    if (title != nullptr) {
        m_title.SetText(title, -1);
        m_title.m_colour = GLcolour4{0xff, 0xff, 0, 0xff};
        m_title.UpdateColour();
    }
    std::vector<std::string> lines;
    m_font->SplitLines(text, 0.09f, 0.85f, lines);
    if (title != nullptr) m_labelCount = 0;
    for (const std::string& line : lines) AddLabel(line.c_str(), colour);
}

void Tooltip::AddLabel(const char* text, GLcolour4 colour) {
    if (m_labelCount == static_cast<int>(m_labels.size())) {   // a new line object
        m_labels.push_back(Label(m_font));
        Label& l = m_labels[m_labelCount];
        l.m_spacing = 2.0f;
        l.m_scale = GLfloat2{0.09f, 0.09f};
        l.m_colour.a = 0;
        l.UpdateColour();
    }
    Label& l = m_labels[m_labelCount];
    l.SetText(text, -1);
    l.m_colour = colour;
    l.UpdateColour();
    ++m_labelCount;
}

void Tooltip::Hide() {
    if (m_visible) m_hiding = true;
}

void Tooltip::HideInstantly() {
    m_visible = false;
    m_hiding = false;
    for (int q = 0; q < 9; ++q) SetQuadColour(q, 0xff, 0xff, 0xff, 0);
    m_title.m_colour.a = 0;
    m_title.UpdateColour();
    for (int i = 0; i < m_labelCount; ++i) {
        m_labels[i].m_colour.a = 0;
        m_labels[i].UpdateColour();
    }
}

// Fades in to alpha 240 (8 x 255 per second), or out while hiding; the frame's first vertex holds
// the current alpha.
void Tooltip::Process(float dt) {
    if (!m_visible) return;
    int alpha;
    float step = dt * 255.0f;
    if (!m_hiding) {
        float a = static_cast<float>(static_cast<int>(m_vertices[0].colour.a)) + step * 8.0f;
        alpha = ClampI(FloatToIntArm(a), 0, 0xf0);
    } else {
        float a = static_cast<float>(static_cast<unsigned>(m_vertices[0].colour.a)) + step * -8.0f;
        alpha = FloatToIntArm(a);
    }
    if (alpha < 1) {
        m_hiding = false;
        m_visible = false;
        return;
    }
    uint8_t a8 = static_cast<uint8_t>(alpha);
    for (int v = 0; v < 9 * 4; ++v) m_vertices[v].colour.a = a8;
    m_title.m_colour.a = a8;
    m_title.UpdateColour();
    for (int i = 0; i < m_labelCount; ++i) {
        m_labels[i].m_colour.a = a8;
        m_labels[i].UpdateColour();
    }
}

void Tooltip::Render() {
    sdlPushMatrix();
    sdlTranslatef(m_position.x, m_position.y, 0.0f);
    sdlBindTexture(GL_TEXTURE_2D, 1);
    sdlVertexPointer(2, GL_FLOAT, sizeof(QuadVertex), &m_vertices[0].position);
    sdlColorPointer(4, GL_UNSIGNED_BYTE, sizeof(QuadVertex), &m_vertices[0].colour);
    sdlTexCoordPointer(2, GL_FLOAT, sizeof(QuadVertex), &m_vertices[0].uv);
    for (int i = 0; i < 0x24; i += 4) sdlDrawArrays(GL_TRIANGLE_FAN, i, 4);
    m_title.Render();
    for (int i = 0; i < m_labelCount; ++i) m_labels[i].Render();
    sdlPopMatrix();
}
