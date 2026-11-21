// QuadStrip: a row-wise layout of Quads (menu frames, buttons, the rune markers): each quad has a
// size policy and lies in a row; UpdateSize sizes the quads from their texture rectangles (or the
// strip's fixed total size), SetPosition lays them out left to right, row by row. 0x4c bytes in the
// original (four std::vectors, then the scalars below).
#pragma once

#include <vector>

#include "dr_types.h"
#include "quad.h"

// How a quad of a strip is sized (QuadSizePolicy; values from the code that uses them):
// 0 natural width, the rest of the fixed height; 1 the rest of the fixed width, natural (or given)
// height; 2 natural width and (given) height; 3 the rest of the fixed size in both directions.
class QuadStrip {
public:
    QuadStrip();                          // 0x0005b5f4
    QuadStrip(const QuadStrip&) = default;   // 0x0002b480 (copies everything)
    QuadStrip& operator=(const QuadStrip&) = default;
    ~QuadStrip() {}                       // 0x0005c0cc

    void AddQuad();                                                            // 0x0005bfa0: policy 3, no texture
    void AddQuad(const GLfloat2& topLeft, const GLfloat2& bottomRight, int policy, float height);   // 0x0005c240 (texture pixels; height < 0: from the texture)
    void SetUV(const GLfloat2& topLeft, const GLfloat2& bottomRight);          // 0x0005c1f0: of the last quad
    void UpdateSize();                                                         // 0x0005bbc4
    GLfloat2 GetFixedSize(int row, int column);                               // 0x0005ba3c
    GLfloat2 GetSize();                                                        // 0x0005bb98
    GLfloat2 GetMiddle();                                                      // 0x00057650
    void SetPosition(GLfloat2 position);                                       // 0x0005b980: top left
    void SetPositionCentre(GLfloat2 centre);                                   // 0x0005c134
    void AlignWith(QuadStrip& other);                                          // 0x0005c190: centred on it
    void Mirror();                                                             // 0x0005b8e0: flips every quad's texture horizontally
    void SetColour(uint8_t r, uint8_t g, uint8_t b, uint8_t a);                // 0x0005b82c
    void SetColour(const GLcolour4& colour);                                   // 0x00056144
    void SetAlpha(uint8_t alpha);                                              // 0x0005b7e4
    bool ContainsPoint(const GLfloat2& point);                                 // 0x0005b790
    void Render();                                                             // 0x0005b898

    std::vector<Quad> m_quads;        // 0x0
    std::vector<int> m_policies;      // 0xc (QuadSizePolicy)
    std::vector<float> m_heights;     // 0x18: < 0: from the texture
    std::vector<int> m_rows;          // 0x24: row of each quad
    int m_currentRow = 0;             // 0x30: row of the quads added next
    int m_textureSize = 0x200;        // 0x34: texture size in pixels (UVs = pixels / size)
    GLfloat2 m_size{0.0f, 0.0f};      // 0x38: fixed total size (used with m_fixedSize)
    bool m_fixedSize = false;         // 0x40
    GLfloat2 m_position{0.0f, 0.0f};  // 0x44: top left
};
