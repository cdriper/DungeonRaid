#include "label.h"

#include <cstring>

#include "dr_gl.h"
#include "font.h"

namespace {
const GLcolour4 kWhite = {0xff, 0xff, 0xff, 0xff};
}

Label::Label() : Label(nullptr) {}

Label::Label(const Font* font)
    : m_glyphCount(0),
      m_font(font),
      m_position{0.5f, 0.75f},
      m_scale{1.0f, 1.0f},
      m_rotation(0.0f),
      m_colour(kWhite),
      m_width(0.0f),
      m_spacing(1.0f) {}

Label::Label(const Label& other) = default;

Label::~Label() = default;

// Quirks kept from the original:
//  - the early return compares the whole new string with the stored text, whatever maxLength is;
//  - maxLength may exceed strlen(text); the characters after the NUL are then laid out too;
//  - new glyph Quads are push_back'ed for indices m_glyphCount..count-1 even when the vector
//    already holds Quads there (left over from a longer text); the existing Quads at those
//    indices are then recoloured and reused, the new ones stay unused at the end;
//  - a glyph's UV/size/offset are only rebuilt where the character differs from the old text.
void Label::SetText(const char* text, int maxLength) {
    size_t length = std::strlen(text);
    if (length == m_text.size() && std::memcmp(m_text.data(), text, length) == 0) {
        return;
    }
    int count = maxLength < 0 ? static_cast<int>(length) : maxLength;
    for (int i = m_glyphCount; i < count; ++i) {
        m_glyphs.push_back(Quad());
        m_glyphs[i].SetColour(m_colour);
    }
    m_glyphCount = count;
    m_width = 0.0f;
    float x = 0.0f;
    for (int i = 0; i < count; ++i) {
        char c = text[i];
        char shown = (c == '\t') ? ' ' : c;
        Quad& glyph = m_glyphs[i];
        if (static_cast<size_t>(i) >= m_text.size() || m_text[i] != c) {
            glyph.SetUV(m_font->GetTopLeft(shown), m_font->GetBottomRight(shown));
            glyph.SetSize(m_font->GetSize(shown));
            glyph.SetOffset(m_font->GetOffset(shown));
        }
        float half = m_font->GetSize(shown).x * 0.5f;
        float centre = x + half;
        glyph.m_position = {centre, 0.0f};
        x = m_font->GetAdvance(shown, m_spacing) + centre;
    }
    m_width = x;
    m_text.assign(text, static_cast<size_t>(count));
}

void Label::UpdateColour() {
    for (Quad& glyph : m_glyphs) {
        glyph.SetColour(m_colour);
    }
}

// Signed comparisons, as in the original: from .. min(to, number of Quads).
void Label::SetSubColour(unsigned from, unsigned to, const GLcolour4& colour) {
    for (int i = static_cast<int>(from);; ++i) {
        int end = static_cast<int>(m_glyphs.size());
        if (static_cast<int>(to) < end) {
            end = static_cast<int>(to);
        }
        if (i >= end) {
            break;
        }
        m_glyphs[i].SetColour(colour);
    }
}

void Label::Render() {
    if (m_glyphCount == 0) {
        return;
    }
    sdlBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(m_font->m_texture));
    sdlPushMatrix();
    sdlTranslatef(m_position.x, m_position.y, 0.0f);
    sdlScalef(m_scale.x, m_scale.y, 0.0f);
    sdlRotatef(m_rotation, 0.0f, 0.0f, 1.0f);
    for (int i = 0; i < m_glyphCount; ++i) {
        m_glyphs[i].Render();
    }
    sdlPopMatrix();
}
