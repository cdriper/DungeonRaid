// Particles: shard bursts. See particles.h. Float details follow the disassembly (single precision
// with the original's operation order; the corner tests of InitialiseFromTile are in double).
#include "particles.h"

#include <cmath>

#include "dr_gl.h"
#include "rand.h"
#include "tile.h"
#include "util.h"

namespace {
constexpr double kTwoPi = 6.283185307179586;
constexpr double kQuarterPi = 0.7853981633974483;
// the square's corners as angles: pi/4, 3pi/4, 5pi/4, 7pi/4
constexpr double kCorners[4] = {0.7853981633974483, 2.356194490192345, 3.9269908169872414, 5.497787143782138};
constexpr float kTwoPiF = 6.2831855f;     // 0x40c90fdb
constexpr float kStep = 0.62831855f;      // 0x3f20d97c: 2pi / 10
constexpr float kHalfStep = 0.31415927f;  // 0x3ea0d97c
}  // namespace

void CastRayOntoSquare(float angle, float half, float u0, float u1, float v0, float v1, const GLfloat2& centre,
                       QuadVertex& out) {
    float c = CosF(angle);
    float s = SinF(angle);
    float x, y;
    float ac = c < 0.0f ? -c : c;
    float as = s < 0.0f ? -s : s;
    if (ac > as) {   // leaves through the left or right side
        y = (half / ac) * s;
        x = (c < 0.0f) ? -half : half;
    } else {         // through the top or bottom (also for NaN)
        x = (half / as) * c;
        y = (s < 0.0f) ? -half : half;
    }
    float u = Interpolate(u0, u1, (1.0f - x / half) * 0.5f);
    float v = Interpolate(v0, v1, (1.0f - y / half) * 0.5f);
    out.uv.x = u;
    out.position.y = y + centre.y;
    out.position.x = centre.x + x;
    out.uv.y = v;
}

// Life 0.3 s. The tile's pieces in white; the sparks at the armour (blue-grey), health (red) or
// attack (white) label. Unknown types take their colour from uninitialised registers in the
// original (the game uses 0..3); the port uses white.
void Particles::InitialiseFromTile(Tile* tile, int type) {
    m_age = 0.0f;
    m_active = true;
    m_lifetime = 0.3f;
    uint8_t r = 0xff, g = 0xff, b = 0xff;
    if (type == 0) {
        InitialiseFromTile(tile);
    } else if (type == 1) {
        InitialiseAtPosition(tile->GetDefenceLabelPosition(), 0.01f);
        r = 0xad;
        g = 0xb1;
    } else if (type == 2) {
        InitialiseAtPosition(tile->GetHealthLabelPosition(), 0.01f);
        r = 0xd7;
        g = 0;
        b = 0x21;
    } else if (type == 3) {
        InitialiseAtPosition(tile->GetAttackLabelPosition(), 0.01f);
    }
    for (QuadVertex& v : m_vertices) v.colour = GLcolour4{r, g, b, 0xff};
}

// Ten wedges around a jittered centre: wedge i spans the ray angles a(i-1)..a(i) (a = random start
// + i * 2pi/10 + a little jitter), its outer edge on the tile's square (with the square's corner as
// the middle vertex when the wedge spans one, else the edge's midpoint), its tip at the centre.
// Each flies outwards along its middle angle at 1..2 /s, slowing by 3..10 /s^2.
void Particles::InitialiseFromTile(Tile* tile) {
    GLfloat2 pos = tile->m_position;
    float half = tile->m_size.x * 0.5f;
    float u0, v0, u1, v1;
    tile->GetUV(&u0, &v0, &u1, &v1);
    float q = half * 0.25f;
    float dx = Rand(-q, q, RAND_COSMETIC);
    float dy = Rand(-q, q, RAND_COSMETIC);
    float cu = Interpolate(u0, u1, (1.0f - dx / half) * 0.5f);
    float cv = Interpolate(v0, v1, (1.0f - dy / half) * 0.5f);
    float centreX = dx + pos.x;
    float centreY = dy + pos.y;
    float start = Rand(0.0f, kTwoPiF, RAND_COSMETIC);
    float prev = start;
    CastRayOntoSquare(start, half, u0, u1, v0, v1, pos, m_vertices[38]);   // (scratch slot, overwritten below)
    m_vertices[0].position = m_vertices[38].position;
    m_vertices[0].uv = m_vertices[38].uv;
    float sx = 1.0f, sy = 1.0f;   // (registers carried over from the previous corner in the original)
    for (int i = 0; i < 10; ++i) {
        float jitter = Rand(-0.20943952f, 0.20943952f, RAND_COSMETIC);
        float a = (start + static_cast<float>(i + 1) * kStep) + jitter;
        float angle = a;
        if (static_cast<double>(a) > kTwoPi) {
            angle = static_cast<float>(static_cast<double>(a) - kTwoPi);
            prev = static_cast<float>(static_cast<double>(prev) - kTwoPi);
        }
        double prevD = static_cast<double>(prev);
        bool corner = false;
        for (double c : kCorners) {
            if (prevD < c && static_cast<double>(angle) > c) {
                corner = true;
                break;
            }
        }
        QuadVertex* v = &m_vertices[i * 4];
        CastRayOntoSquare(angle, half, u0, u1, v0, v1, pos, v[2]);
        v[3].position = GLfloat2{centreX, centreY};
        v[3].uv = GLfloat2{cu, cv};
        if (i != 0) {
            v[0].position = m_vertices[i * 4 - 2].position;
            v[0].uv = m_vertices[i * 4 - 2].uv;
        }
        if (corner) {
            float sector = static_cast<float>(prevD / kQuarterPi);
            if (sector < 1.0f) {
                sx = 1.0f;
                sy = 1.0f;
            } else if (sector < 3.0f) {
                sx = -1.0f;
                sy = 1.0f;
            } else if (sector < 5.0f) {
                sx = -1.0f;
                sy = -1.0f;
            } else if (sector < 7.0f) {
                sx = 1.0f;
                sy = -1.0f;
            }
            v[1].position.x = pos.x + sx * half;
            v[1].position.y = pos.y + sy * half;
            v[1].uv.x = Interpolate(u0, u1, (1.0f - sx) * 0.5f);
            v[1].uv.y = Interpolate(v0, v1, (1.0f - sy) * 0.5f);
        } else {
            v[1].position.x = (v[0].position.x + v[2].position.x) * 0.5f;
            v[1].position.y = (v[0].position.y + v[2].position.y) * 0.5f;
            v[1].uv.x = (v[0].uv.x + v[2].uv.x) * 0.5f;
            v[1].uv.y = (v[0].uv.y + v[2].uv.y) * 0.5f;
        }
        float mid = (prev + angle) * 0.5f;
        prev = a;
        m_velocity[i].x = CosF(mid);
        m_velocity[i].y = SinF(mid);
        float speed = Rand(1.0f, 2.0f, RAND_COSMETIC);
        m_velocity[i].x = speed * m_velocity[i].x;
        m_velocity[i].y = speed * m_velocity[i].y;
        m_acceleration[i] = Rand(-3.0f, -10.0f, RAND_COSMETIC);
    }
}

// Ten small triangles fanning out from the position: three rays at a, a + pi/10, a + pi/5 with
// random lengths radius..2 radius; they drift along the middle ray at 0.1..0.3 times its length per
// radius, slowing by 0.5..3.
void Particles::InitialiseAtPosition(const GLfloat2& position, float radius) {
    float start = Rand(0.0f, kTwoPiF, RAND_COSMETIC);
    float radius2 = radius + radius;
    for (int i = 0; i < 10; ++i) {
        float a = start + static_cast<float>(i) * kStep;
        float b = a + kHalfStep;
        float c = a + kStep;
        QuadVertex* v = &m_vertices[i * 4];
        v[0].position.x = CosF(a) * Rand(radius, radius2, RAND_COSMETIC);
        v[0].position.y = SinF(a) * Rand(radius, radius2, RAND_COSMETIC);
        v[0].uv = GLfloat2{0.01953125f, 0.0615234375f};
        v[1].position.x = CosF(b) * Rand(radius, radius2, RAND_COSMETIC);
        v[1].position.y = SinF(b) * Rand(radius, radius2, RAND_COSMETIC);
        v[1].uv = GLfloat2{0.0244140625f, 0.0595703125f};
        v[2].position.x = CosF(c) * Rand(radius, radius2, RAND_COSMETIC);
        v[2].position.y = SinF(c) * Rand(radius, radius2, RAND_COSMETIC);
        v[2].uv = GLfloat2{0.029296875f, 0.0615234375f};
        v[3].position = GLfloat2{0.0f, 0.0f};
        v[3].uv = GLfloat2{0.0244140625f, 0.0673828125f};
        m_velocity[i].x = v[1].position.x / radius;
        m_velocity[i].y = v[1].position.y / radius;
        float f = Rand(0.1f, 0.3f, RAND_COSMETIC);
        m_velocity[i].x = f * m_velocity[i].x;
        m_velocity[i].y = f * m_velocity[i].y;
        m_acceleration[i] = Rand(-0.5f, -3.0f, RAND_COSMETIC);
    }
    for (QuadVertex& v : m_vertices) {
        v.position.x = v.position.x + position.x;
        v.position.y = v.position.y + position.y;
    }
}

// Moves the shards (speed changes by the acceleration, direction kept) and fades them by
// 1 - (age / lifetime)^2; inactive after the lifetime.
void Particles::Process(float dt) {
    float age = dt + m_age;
    m_age = age;
    if (age > m_lifetime) {   // (NaN keeps it alive, as the original's ble)
        m_active = false;
        return;
    }
    float f = Sqr(age / m_lifetime);
    uint8_t alpha = static_cast<uint8_t>(FloatToUintArm((1.0f - f) * 255.0f));
    for (int i = 0; i < 10; ++i) {
        float vx = m_velocity[i].x;
        float vy = m_velocity[i].y;
        float length = std::sqrt(vy * vy + vx * vx);
        float nx = vx / length;
        float ny = vy / length;
        m_velocity[i] = GLfloat2{nx, ny};
        float speed = length + dt * m_acceleration[i];
        vx = nx * speed;
        vy = speed * ny;
        m_velocity[i] = GLfloat2{vx, vy};
        float dx = dt * vx;
        float dy = dt * vy;
        for (int k = 0; k < 4; ++k) {
            QuadVertex& v = m_vertices[i * 4 + k];
            v.position.x = v.position.x + dx;
            v.position.y = v.position.y + dy;
            v.colour.a = alpha;
        }
    }
}

void Particles::Render() {
    sdlVertexPointer(2, GL_FLOAT, sizeof(QuadVertex), &m_vertices[0].position);
    sdlColorPointer(4, GL_UNSIGNED_BYTE, sizeof(QuadVertex), &m_vertices[0].colour);
    sdlTexCoordPointer(2, GL_FLOAT, sizeof(QuadVertex), &m_vertices[0].uv);
    for (int i = 0; i < 10; ++i) sdlDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
}
