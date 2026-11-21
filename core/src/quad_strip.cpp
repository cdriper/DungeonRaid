// QuadStrip: see quad_strip.h. Sizes in screen units: texture pixels * texture size / 320.
#include "quad_strip.h"

QuadStrip::QuadStrip() {}

void QuadStrip::AddQuad() {
    m_quads.push_back(Quad());
    m_policies.push_back(3);
    m_heights.push_back(-1.0f);
    m_rows.push_back(m_currentRow);
    m_quads.back().SetUV(GLfloat2{0.0f, 0.0f}, GLfloat2{0.0f, 0.0f});
    UpdateSize();
}

void QuadStrip::AddQuad(const GLfloat2& topLeft, const GLfloat2& bottomRight, int policy, float height) {
    m_quads.push_back(Quad());
    m_policies.push_back(policy);
    m_heights.push_back(height);
    m_rows.push_back(m_currentRow);
    float size = static_cast<float>(m_textureSize);
    GLfloat2 tl{topLeft.x / size, topLeft.y / size};
    GLfloat2 br{bottomRight.x / size, bottomRight.y / size};
    m_quads.back().SetUV(tl, br);
    UpdateSize();
}

void QuadStrip::SetUV(const GLfloat2& topLeft, const GLfloat2& bottomRight) {
    float size = static_cast<float>(m_textureSize);
    GLfloat2 tl{topLeft.x / size, topLeft.y / size};
    GLfloat2 br{bottomRight.x / size, bottomRight.y / size};
    m_quads.back().SetUV(tl, br);
}

// Each quad: natural size from its texture rectangle (or its given height); the remainder is the
// fixed size minus the fixed size of the quad's row/column (GetFixedSize); then the policy picks.
void QuadStrip::UpdateSize() {
    int column = -1;
    for (unsigned i = 0; i < m_quads.size(); ++i) {
        int row = m_rows[i];
        column = (i == 0 || row == m_rows[i - 1]) ? column + 1 : 0;
        GLfloat2 fixed = GetFixedSize(row, column);
        float restX = m_size.x - fixed.x;
        float restY = m_size.y - fixed.y;
        Quad& q = m_quads[i];
        float tex = static_cast<float>(m_textureSize);
        float w = (tex * (q.m_vertices[3].uv.x - q.m_vertices[0].uv.x)) / 320.0f;
        float h = m_heights[i];
        if (h < 0.0f) h = (tex * (q.m_vertices[3].uv.y - q.m_vertices[0].uv.y)) / 320.0f;
        switch (m_policies[i]) {
            case 2: q.SetSize(GLfloat2{w, h}); break;
            case 3: q.SetSize(GLfloat2{restX, restY}); break;
            case 1: q.SetSize(GLfloat2{restX, h}); break;
            default: q.SetSize(GLfloat2{w, restY}); break;
        }
    }
    SetPosition(m_position);
}

// The natural size of the quads that count for (row, column): any quad when row or column is
// negative, else those in the row or in the column (an OR, as in the original). Widths of policies
// 0 and 2 add up; heights of policies 1 and 2 add up (given or natural). For (-1, -1) the height is
// the last quad's natural height instead.
GLfloat2 QuadStrip::GetFixedSize(int row, int column) {
    GLfloat2 out{0.0f, 0.0f};
    int col = -1;
    for (unsigned i = 0; i < m_quads.size(); ++i) {
        col = (i == 0 || m_rows[i] == m_rows[i - 1]) ? col + 1 : 0;
        if (!(row < 0 || m_rows[i] == row || column < 0 || column == col)) continue;
        Quad& q = m_quads[i];
        float tex = static_cast<float>(m_textureSize);
        float h = (tex * (q.m_vertices[3].uv.y - q.m_vertices[0].uv.y)) / 320.0f;
        int policy = m_policies[i];
        if (row == -1 && column == -1) {
            out.y = h;
        } else if (policy == 2 || policy == 1) {
            float given = m_heights[i];
            out.y = given < 0.0f ? out.y + h : out.y + given;
        }
        if (policy == 2 || policy == 0) {
            float w = (tex * (q.m_vertices[3].uv.x - q.m_vertices[0].uv.x)) / 320.0f;
            out.x = out.x + w;
        }
    }
    return out;
}

GLfloat2 QuadStrip::GetSize() { return m_fixedSize ? m_size : GetFixedSize(-1, -1); }

GLfloat2 QuadStrip::GetMiddle() {
    GLfloat2 size = GetSize();
    return GLfloat2{m_position.x + size.x / 2.0f, m_position.y + size.y / 2.0f};
}

// Quads left to right; a new row starts at the strip's left edge, one quad height lower.
void QuadStrip::SetPosition(GLfloat2 position) {
    m_position = position;
    float x = position.x;
    float y = position.y;
    for (unsigned i = 0; i < m_quads.size(); ++i) {
        Quad& q = m_quads[i];
        q.m_position.x = x + q.m_size.x / 2.0f;
        q.m_position.y = y + q.m_size.y / 2.0f;
        if (i < m_quads.size() - 1 && m_rows[i] != m_rows[i + 1]) {
            x = m_position.x;
            y = y + q.m_size.y;
        } else {
            x = x + q.m_size.x;
        }
    }
}

void QuadStrip::SetPositionCentre(GLfloat2 centre) {
    GLfloat2 size = GetSize();
    SetPosition(GLfloat2{centre.x - size.x / 2.0f, centre.y - size.y / 2.0f});
}

void QuadStrip::AlignWith(QuadStrip& other) {
    GLfloat2 theirs = other.GetSize();
    GLfloat2 mine = GetSize();
    SetPosition(GLfloat2{other.m_position.x + (theirs.x - mine.x) * 0.5f, other.m_position.y + (theirs.y - mine.y) * 0.5f});
}

void QuadStrip::Mirror() {
    for (Quad& q : m_quads) {
        float u0, v0, u1, v1;
        q.GetUV(&u0, &v0, &u1, &v1);
        q.SetUV(GLfloat2{u1, v0}, GLfloat2{u0, v1});
    }
}

void QuadStrip::SetColour(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    for (Quad& q : m_quads) q.SetColour(GLcolour4{r, g, b, a});
}

void QuadStrip::SetColour(const GLcolour4& colour) { SetColour(colour.r, colour.g, colour.b, colour.a); }

void QuadStrip::SetAlpha(uint8_t alpha) {
    for (Quad& q : m_quads) q.SetAlpha(alpha);
}

bool QuadStrip::ContainsPoint(const GLfloat2& point) {
    for (Quad& q : m_quads) {
        if (q.ContainsPoint(point)) return true;
    }
    return false;
}

void QuadStrip::Render() {
    for (Quad& q : m_quads) q.Render();
}
