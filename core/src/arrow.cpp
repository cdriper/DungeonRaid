#include "arrow.h"

#include <cmath>

#include "dr_gl.h"
#include "tile.h"
#include "util.h"

// The original only sets the colours; positions, alpha, UVs and the vertex count stay
// uninitialised until Update. The port zeroes them.
Arrow::Arrow() : m_vertices{}, m_vertexCount(0) {
    SetColour(0x32, 0xff, 0x32);
}

Arrow::~Arrow() = default;

void Arrow::SetColour(unsigned r, unsigned g, unsigned b) {
    for (QuadVertex& v : m_vertices) {
        v.colour.r = static_cast<uint8_t>(r);
        v.colour.g = static_cast<uint8_t>(g);
        v.colour.b = static_cast<uint8_t>(b);
    }
}

// No bounds check, as in the original.
void Arrow::AddVertex(const GLfloat2& position, uint8_t alpha, float u, float v) {
    QuadVertex& vertex = m_vertices[m_vertexCount++];
    vertex.position = position;
    vertex.colour.a = alpha;
    vertex.uv = {u, v};
}

namespace {
// constants from the literal pools (decimal forms round-trip to these bits)
const float kHalfWidth = 0.0140624996f;      // 0x3c666666: half the arrow's width
const float kRoot2 = 1.41421354f;            // 0x3fb504f3: 90-degree corner (1 / cos 45)
const float k45Scale = 1.08239222f;          // 0x3f8a8bd4: 45-degree turn (1 / cos 22.5)
const float k135Scale = 2.61312604f;         // 0x40273d75: 135-degree turn (1 / cos 67.5)
const float k45Inset = 0.00582487928f;       // 0x3bbedea1: kHalfWidth * tan 22.5
const float k135Inset = 0.0339498781f;       // 0x3d0b0f07: kHalfWidth * tan 67.5
// texture coordinates (1024-pixel texture)
const float kU0 = 0.263671875f;              // 270: the shaft
const float kV0 = 0.35546875f;               // 364: one edge of the shaft
const float kV1 = 0.3623046875f;             // 371: the other edge

// A unit direction component rounded the original's way: beyond +-0.1 to +-1, else kept as it is.
float Round01(float x) {
    if (x > 0.1f) return 1.0f;
    if (x < -0.1f) return -1.0f;
    return x;
}
}  // namespace

// Builds the arrow along the chain: a tail piece on the first tile, an arrow head on the last, and
// on each tile in between either a straight piece or a corner piece for a 45-, 90- or 135-degree
// turn (directions compared after Round01; `flip` picks the side of the turn). Every tile closes
// the previous quad with two vertices and opens the next one with two more. Alpha rises from 100
// along the chain. Cosmetic only: no game state, no random numbers.
void Arrow::Update(const std::list<Tile*>& chain) {
    m_vertexCount = 0;
    const unsigned count = static_cast<unsigned>(chain.size());
    if (count < 2) return;
    const Tile* prev = nullptr;
    int index = 0;
    for (auto it = chain.begin(); it != chain.end();) {
        const Tile* tile = *it;
        ++it;
        const Tile* next = it == chain.end() ? nullptr : *it;
        ++index;
        const float t = static_cast<float>(index) / static_cast<float>(count);
        const uint8_t alpha = static_cast<uint8_t>(FloatToUintArm(100.0f + t * 100.0f));
        const GLfloat2 p = tile->m_position;
        auto add = [&](const GLfloat2& pos, float u, float v) { AddVertex(pos, alpha, u, v); };
        // the shaft: closes the previous quad (-N, +N) after the caller's (+N, -N)
        auto shaftOpen = [&](float nx, float ny) {
            add(GLfloat2{p.x - nx, p.y - ny}, kU0, kV1);
            add(GLfloat2{p.x + nx, p.y + ny}, kU0, kV0);
        };
        if (prev == nullptr || next == nullptr) {
            float dx, dy;
            if (prev == nullptr) {
                dx = next->m_position.x - p.x;
                dy = next->m_position.y - p.y;
            } else {
                dx = p.x - prev->m_position.x;
                dy = p.y - prev->m_position.y;
            }
            float len = std::sqrt(dy * dy + dx * dx);
            float uy = dy / len;
            float ux = dx / len;
            float nx = -(uy * kHalfWidth);   // the perpendicular, half the width long
            float ny = ux * kHalfWidth;
            if (prev == nullptr) {   // the tail: a quad behind the first tile
                float ddx = ux * kHalfWidth;
                float ddy = uy * kHalfWidth;
                add(GLfloat2{(p.x - ddx) - nx, (p.y - ddy) - ny}, 0.2939453125f, kV0);
                add(GLfloat2{(p.x - ddx) + nx, (p.y - ddy) + ny}, 0.2939453125f, kV1);
                add(GLfloat2{p.x + nx, p.y + ny}, 0.2978515625f, kV1);
                add(GLfloat2{p.x - nx, p.y - ny}, 0.2978515625f, kV0);
                shaftOpen(nx, ny);
            } else {                 // the head: closes the shaft, then a quad around the last tile
                add(GLfloat2{p.x + nx, p.y + ny}, kU0, kV0);
                add(GLfloat2{p.x - nx, p.y - ny}, kU0, kV1);
                float k = 0.025f / std::sqrt(ny * ny + nx * nx) + 1.0f;
                float hx = k * nx;
                float hy = k * ny;
                add(GLfloat2{p.x - hx, p.y - hy}, 0.2998046875f, 0.3564453125f);
                add(p, 0.30908203125f, 0.3564453125f);
                float negHy = -hy;
                add(GLfloat2{hx + p.x, hy + p.y}, 0.318359375f, 0.3564453125f);
                add(GLfloat2{p.x - negHy, p.y - hx}, 0.30908203125f, 0.3662109375f);
            }
            prev = tile;
            continue;
        }
        // a tile inside the chain: a = direction from the previous tile, b = to the next one
        float dy1 = p.y - prev->m_position.y;
        float dx1 = p.x - prev->m_position.x;
        float len1 = std::sqrt(dy1 * dy1 + dx1 * dx1);
        float ax = dx1 / len1;
        float ay = dy1 / len1;
        float dy2 = next->m_position.y - p.y;
        float dx2 = next->m_position.x - p.x;
        float len2 = std::sqrt(dy2 * dy2 + dx2 * dx2);
        float by = dy2 / len2;
        float bx = dx2 / len2;
        float sy = ay + by;
        float sx = ax + bx;
        float slen = std::sqrt(sy * sy + sx * sx);
        float nx = -((sy / slen) * kHalfWidth);   // perpendicular to the bisector
        float ny = (sx / slen) * kHalfWidth;
        const float rax = Round01(ax), rbx = Round01(bx), ray = Round01(ay), rby = Round01(by);
        enum { STRAIGHT, CORNER90, TURN45, TURN135 } kind;
        bool flip;
        if (rbx == rax && rby == ray) {
            kind = STRAIGHT;
            flip = false;
        } else if (rby == rax && -rbx == ray) {
            kind = CORNER90;
            flip = true;
        } else if (-rby == rax && rbx == ray) {
            kind = CORNER90;
            flip = rby == rax && -rbx == ray;
        } else if (rbx == rax || rby == ray) {
            kind = TURN45;
            if (rax == 0.0f) {
                flip = rbx != ray;
            } else if (ray == 0.0f) {
                flip = rby == rax;
            } else {
                flip = (rax == ray ? rbx : rby) == 0.0f;
            }
        } else {
            kind = TURN135;
            if (rax == 0.0f) {
                flip = rbx != ray;
            } else if (ray == 0.0f) {
                flip = rby == rax;
            } else {
                flip = (rax == ray ? rbx : rby) != 0.0f;
            }
        }
        if (kind == STRAIGHT) {
            add(GLfloat2{p.x + nx, p.y + ny}, kU0, kV0);
            add(GLfloat2{p.x - nx, p.y - ny}, kU0, kV1);
            shaftOpen(nx, ny);
            prev = tile;
            continue;
        }
        // the vertices of a corner piece (names as in the comments below); `shaft` decides the
        // two vertices that close the previous quad and the two that open the next one
        GLfloat2 a, b, c, d;
        if (kind == CORNER90) {
            nx = nx * kRoot2;
            ny = ny * kRoot2;
            float awx = ax * kHalfWidth, awy = ay * kHalfWidth;
            float bwx = bx * kHalfWidth, bwy = by * kHalfWidth;
            float baseX = p.x - awx, baseY = p.y - awy;
            GLfloat2 q1{bwx + baseX, bwy + baseY};   // inner corner side
            GLfloat2 q2{baseX - bwx, baseY - bwy};
            if (flip) {
                nx = -nx;
                ny = -ny;
            }
            GLfloat2 outer{nx + p.x, ny + p.y};
            float ex = bwx + p.x, ey = bwy + p.y;
            GLfloat2 far{awx + ex, awy + ey};
            add(flip ? q1 : q2, kU0, kV0);
            add(flip ? q2 : q1, kU0, kV1);
            add(q1, 0.267578125f, kV1);
            add(q2, 0.267578125f, kV0);
            add(outer, 0.2744140625f, kV0);
            add(far, 0.2744140625f, kV1);
            add(flip ? far : q1, kU0, kV1);
            add(flip ? q1 : far, kU0, kV0);
            prev = tile;
            continue;
        }
        float scale = kind == TURN45 ? k45Scale : k135Scale;
        float inset = kind == TURN45 ? k45Inset : k135Inset;
        float cnx = nx * scale, cny = ny * scale;          // towards the outside of the turn
        float aix = ax * inset, aiy = ay * inset;
        float awx = ax * kHalfWidth, awy = -(ay * kHalfWidth);
        float bix = bx * inset, biy = by * inset;
        float bwx = -(by * kHalfWidth), bwy = bx * kHalfWidth;
        float px = flip ? awy : -awy, py = flip ? awx : -awx;
        b = GLfloat2{px + (p.x - aix), (p.y - aiy) + py};   // (sp+0x88)
        float baseX = p.x - aix, baseY = p.y - aiy;
        if (!flip) {
            a = GLfloat2{baseX + awy, baseY + awx};          // (sp+0x80)
        } else {
            a = GLfloat2{baseX - awy, baseY - awx};
            cnx = -cnx;
            cny = -cny;
        }
        c = GLfloat2{p.x + cnx, p.y + cny};                  // (sp+0x78) the outer corner
        float ex = bix + p.x, ey = biy + p.y;
        d = flip ? GLfloat2{ex - bwx, ey - bwy} : GLfloat2{bwx + ex, bwy + ey};   // (sp+0x70)
        add(flip ? b : a, kU0, kV0);
        add(flip ? a : b, kU0, kV1);
        if (kind == TURN45) {
            add(b, kU0, 0.37109375f);
            add(a, kU0, 0.3642578125f);
            add(c, 0.2666015625f, 0.3642578125f);
            add(d, 0.2685546875f, 0.3671875f);
        } else {
            add(b, 0.2763671875f, kV1);
            add(a, 0.28125f, 0.357421875f);
            add(c, 0.2919921875f, 0.369140625f);
            add(d, 0.2763671875f, 0.369140625f);
        }
        add(flip ? d : b, kU0, kV1);
        add(flip ? b : d, kU0, kV0);
        prev = tile;
    }
}

// The tail and head geometry is Update's; the joint on the last tile is a quad spanning the
// incoming and outgoing shaft ends (covers the gap of any turn angle).
bool Arrow::UpdateLive(const std::list<Tile*>& chain, const GLfloat2& pointer) {
    Update(chain);
    if (chain.empty()) return false;
    const unsigned count = static_cast<unsigned>(chain.size());
    const GLfloat2 p = chain.back()->m_position;
    float dx = pointer.x - p.x;
    float dy = pointer.y - p.y;
    float len = std::sqrt(dy * dy + dx * dx);
    if (len < 0.04f) return count > 1;   // the pointer is on the last tile: the usual arrow
    // the extra vertices: -4 (head) +4 (joint) +2 (open) +6 (head), or 6 (tail) +6 (head)
    if (m_vertexCount + 12 > kMaxVertices) return count > 1;
    const uint8_t alpha = 200;
    auto add = [&](const GLfloat2& pos, float u, float v) { AddVertex(pos, alpha, u, v); };
    float ux = dx / len, uy = dy / len;
    float nx = -(uy * kHalfWidth);
    float ny = ux * kHalfWidth;
    if (count > 1) {
        m_vertexCount -= 4;   // drop the head fan (its two shaft-closing vertices stay)
        auto prevIt = chain.end();
        --prevIt;
        --prevIt;
        const GLfloat2 q = (*prevIt)->m_position;
        float ix = p.x - q.x, iy = p.y - q.y;
        float ilen = std::sqrt(iy * iy + ix * ix);
        float inx = -(iy / ilen * kHalfWidth);
        float iny = ix / ilen * kHalfWidth;
        add(GLfloat2{p.x + inx, p.y + iny}, kU0, kV0);
        add(GLfloat2{p.x + nx, p.y + ny}, kU0, kV0);
        add(GLfloat2{p.x - inx, p.y - iny}, kU0, kV1);
        add(GLfloat2{p.x - nx, p.y - ny}, kU0, kV1);
    } else {   // the tail, as Update's
        float ddx = ux * kHalfWidth;
        float ddy = uy * kHalfWidth;
        add(GLfloat2{(p.x - ddx) - nx, (p.y - ddy) - ny}, 0.2939453125f, kV0);
        add(GLfloat2{(p.x - ddx) + nx, (p.y - ddy) + ny}, 0.2939453125f, kV1);
        add(GLfloat2{p.x + nx, p.y + ny}, 0.2978515625f, kV1);
        add(GLfloat2{p.x - nx, p.y - ny}, 0.2978515625f, kV0);
    }
    add(GLfloat2{p.x - nx, p.y - ny}, kU0, kV1);   // open the shaft
    add(GLfloat2{p.x + nx, p.y + ny}, kU0, kV0);
    const GLfloat2 e = pointer;                    // close it and the head, as Update's
    add(GLfloat2{e.x + nx, e.y + ny}, kU0, kV0);
    add(GLfloat2{e.x - nx, e.y - ny}, kU0, kV1);
    float k = 0.025f / std::sqrt(ny * ny + nx * nx) + 1.0f;
    float hx = k * nx;
    float hy = k * ny;
    add(GLfloat2{e.x - hx, e.y - hy}, 0.2998046875f, 0.3564453125f);
    add(e, 0.30908203125f, 0.3564453125f);
    add(GLfloat2{hx + e.x, hy + e.y}, 0.318359375f, 0.3564453125f);
    add(GLfloat2{e.x + hy, e.y - hx}, 0.30908203125f, 0.3662109375f);
    return true;
}

void Arrow::RenderNodes(const std::list<Tile*>& chain) {
    const int kSegments = 16;
    const float kRadius = kHalfWidth * 1.6f;
    const float kRim = 0.004f;
    QuadVertex fan[kSegments + 2];
    const GLcolour4 colour = m_vertices[0].colour;   // the arrow's (green, or a skill's)
    sdlLoadIdentity();
    sdlDisable(GL_TEXTURE_2D);
    sdlVertexPointer(2, GL_FLOAT, sizeof(QuadVertex), &fan[0].position);
    sdlColorPointer(4, GL_UNSIGNED_BYTE, sizeof(QuadVertex), &fan[0].colour);
    sdlTexCoordPointer(2, GL_FLOAT, sizeof(QuadVertex), &fan[0].uv);
    auto circle = [&](const GLfloat2& c, float r, GLcolour4 col) {
        fan[0] = QuadVertex{c, col, {0.0f, 0.0f}};
        for (int i = 0; i <= kSegments; ++i) {
            float a = static_cast<float>(i) * (6.28318531f / static_cast<float>(kSegments));
            fan[i + 1] = QuadVertex{GLfloat2{c.x + r * std::cos(a), c.y + r * std::sin(a)}, col, {0.0f, 0.0f}};
        }
        sdlDrawArrays(GL_TRIANGLE_FAN, 0, kSegments + 2);
    };
    for (const Tile* t : chain) {
        circle(t->m_position, kRadius + kRim, GLcolour4{0, 0, 0, 170});
        circle(t->m_position, kRadius, GLcolour4{colour.r, colour.g, colour.b, 235});
    }
    sdlEnable(GL_TEXTURE_2D);
}

void Arrow::Render() {
    sdlLoadIdentity();
    sdlVertexPointer(2, GL_FLOAT, sizeof(QuadVertex), &m_vertices[0].position);
    sdlColorPointer(4, GL_UNSIGNED_BYTE, sizeof(QuadVertex), &m_vertices[0].colour);
    sdlTexCoordPointer(2, GL_FLOAT, sizeof(QuadVertex), &m_vertices[0].uv);
    if (m_vertexCount > 3) {
        for (int i = 0; i < m_vertexCount / 4; ++i) {
            sdlDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
        }
    }
}
