// Quad: a textured, coloured rectangle drawn as a 4-vertex triangle strip. Base of Tile and
// Token, embedded all over the screens. Layout: re/structs/Quad.json, QuadVertex.json.
#pragma once

#include <cstdint>

#include "dr_types.h"

struct QuadVertex {
    GLfloat2 position;
    GLcolour4 colour;
    GLfloat2 uv;
};

class Quad {
public:
    Quad();                       // 0x0005b250
    Quad(const Quad& other);      // 0x000249b0 (copies everything, the render flags included)
    Quad& operator=(const Quad& other) = default;
    virtual ~Quad();              // 0x0005b334 / 0x0005b5a0

    void SetUV(const GLfloat2& topLeft, const GLfloat2& bottomRight);  // 0x0005b338
    void SetSize(const GLfloat2& size);                                // 0x0005b37c
    void SetOffset(const GLfloat2& offset);                            // 0x0005b3d0
    void SetColour(const GLcolour4& colour);                           // 0x0005b3fc
    void SetAlpha(uint8_t alpha);                                      // 0x0005b4a4
    void CopyFrom(const Quad* other);    // 0x0005b418; NOT the render flags
    void GetUV(float* u0, float* v0, float* u1, float* v1) const;      // 0x0005b474
    bool ContainsPoint(const GLfloat2& point) const;                   // 0x0005b4b4
    virtual void Render();                                             // 0x0005b528 (vtable slot 2)

    QuadVertex m_vertices[4];   // strip order: (-x,-y) (+x,-y) (-x,+y) (+x,+y)
    GLfloat2 m_position;
    GLfloat2 m_size;
    GLfloat2 m_scale;
    float m_rotation;           // degrees
    bool m_useColours;
    bool m_useTexture;
};
