// SimpleLabel: a short floating text made of up to 8 glyph rectangles from the game texture: an
// optional number (digit glyphs, a minus) before and/or after a word picked by the label type
// ("+5 Gold", "Armour -3", ...). Rises and fades out when processed. 0x358 bytes in the original;
// the layout below is the original one (offsets in the comments).
#pragma once

#include "dr_types.h"
#include "quad.h"

class SimpleLabel {
public:
    SimpleLabel();     // 0x0005eba0
    ~SimpleLabel() {}  // 0x0005ebcc

    void Initialise(int type, const GLfloat2& position, int number, float scale);   // 0x0005fbc4 (SimpleLabelType)
    void SetColour(int colour);                                         // 0x0005ec24: index into the colour tables
    void SetNumber(int number);                                         // 0x0005fb88
    void SetPosition(const GLfloat2& position);                         // 0x0005f994
    void SetAlpha(uint8_t alpha);                                       // 0x0005fbb4: all 32 vertices
    void Process(float dt);                                             // 0x0005fc64
    void Render();                                                      // 0x0005fcec

    float GetNumberScale() const;                                       // 0x0005ebd0
    bool UseNumberFront() const;                                        // 0x0005ebec
    bool UseNumberBack() const;                                         // 0x0005ec08
    void SetDefinition(int index, const GLfloat2& topLeft, const GLfloat2& bottomRight, float scale);  // 0x0005ec64
    void CreateTypeDefinition();                                        // 0x0005ed00
    void CreateTypeDefinitionLast();                                    // 0x0005ec9c
    void CreateNumberDefinition(int number);                            // 0x0005f848
    void SetVertex(int index, float x, float y, float u, float v);      // 0x0005f938
    void RegenerateLabel();                                             // 0x0005f9a0

    bool m_active;                  // 0x0
    int m_type;                     // 0x4 (SimpleLabelType)
    QuadVertex m_vertices[32];      // 0x8: 4 per glyph, drawn as triangle fans
    struct GlyphRect {
        GLfloat2 topLeft;           // texture pixels
        GLfloat2 bottomRight;
    };
    GlyphRect m_glyphs[8];          // 0x288
    float m_glyphScales[8];         // 0x308
    int m_glyphCount;               // 0x328
    GLfloat2 m_offset;              // 0x32c: of the current glyph (RegenerateLabel)
    GLfloat2 m_position;            // 0x334
    float m_scale;                  // 0x33c
    float m_numberScale;            // 0x340
    float m_riseSpeed;              // 0x344
    float m_age;                    // 0x348
    float m_lifetime;               // 0x34c
    int m_number;                   // 0x350
    uint8_t m_alpha;                // 0x354: alpha at the start of the fade
};
