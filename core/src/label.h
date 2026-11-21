// Label: one line of bitmap-font text, one Quad per glyph. Layout: re/structs/Label.json.
#pragma once

#include <string>
#include <vector>

#include "dr_types.h"
#include "quad.h"

class Font;

class Label {
public:
    Label();                                  // 0x000526d4
    explicit Label(const Font* font);         // 0x000525e4
    Label(const Label& other);                // 0x00052748
    Label& operator=(const Label& other) = default;
    ~Label();                                 // 0x000527bc

    // Lays out `text` (maxLength < 0: strlen). Does nothing if the text is unchanged.
    void SetText(const char* text, int maxLength = -1);   // 0x00052814
    void UpdateColour();                                  // 0x00052558
    void SetSubColour(unsigned from, unsigned to, const GLcolour4& colour);  // 0x000525a0
    void Render();                                        // 0x00052440

    std::string m_text;
    std::vector<Quad> m_glyphs;   // can hold more Quads than m_glyphCount (see SetText)
    int m_glyphCount;
    const Font* m_font;
    GLfloat2 m_position;
    GLfloat2 m_scale;
    float m_rotation;
    GLcolour4 m_colour;
    float m_width;                // pen position after the last glyph
    float m_spacing;              // passed to Font::GetAdvance for spaces
};
