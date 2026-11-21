#include "menu.h"

// The original leaves the array uninitialised (operator new[]); the port zeroes it.
Menu::Menu(int quadCount) : m_vertices(new QuadVertex[quadCount * 4]()), m_quadCount(quadCount) {}

Menu::~Menu() { delete[] m_vertices; }

void Menu::SetVertex(int index, float x, float y, float u, float v) {
    m_vertices[index].position = GLfloat2{x / 320.0f, y / 320.0f};
    m_vertices[index].uv = GLfloat2{u * 0.0009765625f, v * 0.0009765625f};
}

void Menu::SetVertexPos(int index, float x, float y) { m_vertices[index].position = GLfloat2{x / 320.0f, y / 320.0f}; }

void Menu::SetVertexUV(int index, float u, float v) {
    m_vertices[index].uv = GLfloat2{u * 0.0009765625f, v * 0.0009765625f};
}

void Menu::SetQuad(int quad, const GLfloat2& pos, const GLfloat2& uv, const GLfloat2& size, float scaleX,
                   float scaleY) {
    int i = quad * 4;
    SetVertex(i, pos.x, pos.y, uv.x, uv.y);
    SetVertex(i + 1, pos.x + size.x * scaleX, pos.y, size.x + uv.x, uv.y);
    SetVertex(i + 2, pos.x + size.x * scaleX, pos.y + size.y * scaleY, size.x + uv.x, size.y + uv.y);
    SetVertex(i + 3, pos.x, pos.y + size.y * scaleY, uv.x, size.y + uv.y);
}

void Menu::SetQuad(int quad, const GLfloat2& uv, const GLfloat2& size) {
    int i = quad * 4;
    SetVertex(i, 0.0f, 0.0f, uv.x, uv.y);
    SetVertex(i + 1, size.x, 0.0f, size.x + uv.x, uv.y);
    SetVertex(i + 2, size.x, size.y, size.x + uv.x, size.y + uv.y);
    SetVertex(i + 3, 0.0f, size.y, uv.x, size.y + uv.y);
}

void Menu::SetQuad(int quad, float x, float y, float w, float h) {
    int i = quad * 4;
    SetVertexPos(i, x, y);
    float right = x + w;
    SetVertexPos(i + 1, right, y);
    float bottom = y + h;
    SetVertexPos(i + 2, right, bottom);
    SetVertexPos(i + 3, x, bottom);
}

void Menu::SetQuadColour(int quad, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    for (int k = 0; k < 4; ++k) m_vertices[quad * 4 + k].colour = GLcolour4{r, g, b, a};
}

void Menu::SetQuadPos(int quad, float x, float y) {
    const GLfloat2& tl = m_vertices[quad * 4].position;
    const GLfloat2& br = m_vertices[quad * 4 + 2].position;
    float w = br.x - tl.x;
    float h = br.y - tl.y;
    int i = quad * 4;
    SetVertexPos(i, x, y);
    float right = x + w * 320.0f;
    float bottom = y + h * 320.0f;
    SetVertexPos(i + 1, right, y);
    SetVertexPos(i + 2, right, bottom);
    SetVertexPos(i + 3, x, bottom);
}

void Menu::SetQuadSize(int quad, float w, float h) {
    const GLfloat2& tl = m_vertices[quad * 4].position;
    float x = tl.x * 320.0f;
    float y = tl.y * 320.0f;
    float right = x + w;
    int i = quad * 4;
    SetVertexPos(i, x, y);
    float bottom = y + h;
    SetVertexPos(i + 1, right, y);
    SetVertexPos(i + 2, right, bottom);
    SetVertexPos(i + 3, x, bottom);
}

void Menu::SetQuadUV(int quad, const GLfloat2& uv, const GLfloat2& size) {
    int i = quad * 4;
    SetVertexUV(i, uv.x, uv.y);
    SetVertexUV(i + 1, uv.x + size.x, uv.y);
    SetVertexUV(i + 2, uv.x + size.x, uv.y + size.y);
    SetVertexUV(i + 3, uv.x, uv.y + size.y);
}
