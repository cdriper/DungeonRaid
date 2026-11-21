#include "upgrade.h"

#include "dr_gl.h"

namespace {
const GLcolour4 kWhite = {0xff, 0xff, 0xff, 0xff};
const float kIconSize = 0.153125f;   // 0x3e1ccccd = 49/320 (49-pixel icon on a 320-pixel screen)
}

// The original leaves the icon's texture coordinates and the name uninitialised; the port zeroes them.
Upgrade::Upgrade()
    : m_iconVertices{{{0.0f, 0.0f}, kWhite, {0.0f, 0.0f}},
                     {{kIconSize, 0.0f}, kWhite, {0.0f, 0.0f}},
                     {{0.0f, kIconSize}, kWhite, {0.0f, 0.0f}},
                     {{kIconSize, kIconSize}, kWhite, {0.0f, 0.0f}}},
      m_iconPosition{0.28125f, 0.0f},
      m_name{} {}

Upgrade::~Upgrade() = default;

// left = 1/size + (50.03/size) * col + xOffset/size, top = 1/size + (50.03/size) * row,
// right/bottom = + 49/size (each vmla unfused, each division separate)
void Upgrade::SetIcon(int col, int row, int textureSize, int xOffset) {
    GLfloat2 tl, br;
    GetIconUVs(col, row, textureSize, xOffset, tl, br);
    m_iconVertices[0].uv = tl;
    m_iconVertices[1].uv = {br.x, tl.y};
    m_iconVertices[2].uv = {tl.x, br.y};
    m_iconVertices[3].uv = br;
}

void Upgrade::GetIconUVs(int col, int row, int textureSize, int xOffset, GLfloat2& topLeft, GLfloat2& bottomRight) {
    float size = static_cast<float>(textureSize);
    float step = 50.03f / size;
    float border = 1.0f / size;
    float top = border + step * static_cast<float>(row);
    float left = border + step * static_cast<float>(col);
    left = left + static_cast<float>(xOffset) / size;
    float cell = 49.0f / size;
    topLeft = {left, top};
    bottomRight = {left + cell, cell + top};
}

void Upgrade::RenderIcon(bool extras) {
    sdlPushMatrix();
    sdlTranslatef(m_iconPosition.x, m_iconPosition.y, 0.0f);
    sdlVertexPointer(2, GL_FLOAT, sizeof(QuadVertex), &m_iconVertices[0].position);
    sdlColorPointer(4, GL_UNSIGNED_BYTE, sizeof(QuadVertex), &m_iconVertices[0].colour);
    sdlTexCoordPointer(2, GL_FLOAT, sizeof(QuadVertex), &m_iconVertices[0].uv);
    sdlDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    if (extras) {
        RenderExtras();
    }
    sdlPopMatrix();
}

void Upgrade::RenderExtras() {}
