#include "quad.h"

#include "dr_gl.h"

namespace {
const GLcolour4 kWhite = {0xff, 0xff, 0xff, 0xff};
}

Quad::Quad()
    : m_vertices{{{-0.5f, -0.5f}, kWhite, {0.0f, 0.0f}},
                 {{0.5f, -0.5f}, kWhite, {1.0f, 0.0f}},
                 {{-0.5f, 0.5f}, kWhite, {0.0f, 1.0f}},
                 {{0.5f, 0.5f}, kWhite, {1.0f, 1.0f}}},
      m_position{0.5f, 0.75f},
      m_size{1.0f, 1.0f},
      m_scale{1.0f, 1.0f},
      m_rotation(0.0f),
      m_useColours(true),
      m_useTexture(true) {}

Quad::Quad(const Quad& other) = default;

Quad::~Quad() = default;

void Quad::SetUV(const GLfloat2& topLeft, const GLfloat2& bottomRight) {
    m_vertices[0].uv = {topLeft.x, topLeft.y};
    m_vertices[1].uv = {bottomRight.x, topLeft.y};
    m_vertices[2].uv = {topLeft.x, bottomRight.y};
    m_vertices[3].uv = {bottomRight.x, bottomRight.y};
}

void Quad::SetSize(const GLfloat2& size) {
    m_size = size;
    float hx = size.x * 0.5f;
    float hy = size.y * 0.5f;
    m_vertices[0].position = {-hx, -hy};
    m_vertices[1].position = {hx, -hy};
    m_vertices[2].position = {-hx, hy};
    m_vertices[3].position = {hx, hy};
}

void Quad::SetOffset(const GLfloat2& offset) {
    for (QuadVertex& v : m_vertices) {
        v.position.x = v.position.x + offset.x;
        v.position.y = v.position.y + offset.y;
    }
}

void Quad::SetColour(const GLcolour4& colour) {
    for (QuadVertex& v : m_vertices) {
        v.colour = colour;
    }
}

void Quad::SetAlpha(uint8_t alpha) {
    for (QuadVertex& v : m_vertices) {
        v.colour.a = alpha;
    }
}

void Quad::CopyFrom(const Quad* other) {
    for (int i = 0; i < 4; ++i) {
        m_vertices[i] = other->m_vertices[i];
    }
    m_position = other->m_position;
    m_size = other->m_size;
    m_scale = other->m_scale;
    m_rotation = other->m_rotation;
}

void Quad::GetUV(float* u0, float* v0, float* u1, float* v1) const {
    if (u0) *u0 = m_vertices[0].uv.x;
    if (v0) *v0 = m_vertices[0].uv.y;
    if (u1) *u1 = m_vertices[3].uv.x;
    if (v1) *v1 = m_vertices[3].uv.y;
}

// Inside the scaled corners 0 and 3 (inclusive); any NaN gives false.
bool Quad::ContainsPoint(const GLfloat2& point) const {
    float minX = m_position.x + m_scale.x * m_vertices[0].position.x;
    if (!(point.x >= minX)) return false;
    float maxX = m_position.x + m_scale.x * m_vertices[3].position.x;
    if (!(point.x <= maxX)) return false;
    float minY = m_position.y + m_scale.y * m_vertices[0].position.y;
    if (!(point.y >= minY)) return false;
    float maxY = m_position.y + m_scale.y * m_vertices[3].position.y;
    return point.y <= maxY;
}

void Quad::Render() {
    sdlPushMatrix();
    sdlTranslatef(m_position.x, m_position.y, 0.0f);
    sdlScalef(m_scale.x, m_scale.y, 0.0f);
    sdlRotatef(m_rotation, 0.0f, 0.0f, 1.0f);
    sdlVertexPointer(2, GL_FLOAT, sizeof(QuadVertex), &m_vertices[0].position);
    if (m_useColours) {
        sdlColorPointer(4, GL_UNSIGNED_BYTE, sizeof(QuadVertex), &m_vertices[0].colour);
    }
    if (m_useTexture) {
        sdlTexCoordPointer(2, GL_FLOAT, sizeof(QuadVertex), &m_vertices[0].uv);
    }
    sdlDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    sdlPopMatrix();
}
