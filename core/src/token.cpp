#include "token.h"

#include <cmath>

#include "interface.h"
#include "rand.h"
#include "tile.h"
#include "util.h"

// The original leaves everything but the interface pointer uninitialised; the port zeroes it.
Token::Token(Interface* ui) : m_interface(ui) {}

Token::~Token() = default;

void Token::InitialiseFromTile(Tile* tile, TileType type, const GLfloat2* position, int tokenType) {
    CopyFrom(tile);
    SetColour(GLcolour4{0xff, 0xff, 0xff, 0xff});
    m_type = type;
    m_alive = true;
    m_target = nullptr;
    if (position != nullptr) m_position = *position;
    m_kind = 0;
    if (tile->m_type == TILE_POTION) {
        if (tile->m_manaPotion) {
            m_kind = 1;
            m_value = m_interface->GetSpecialTarget(this);
            if (m_value < 0) {
                m_alive = false;   // no skill to charge
                return;
            }
        } else if (tile->m_acidPotion) {
            m_kind = 2;
        }
    } else if (tokenType == 3) {
        m_kind = 3;
    }
    // a random initial direction of length 0.1
    m_direction.x = Rand(-0.1f, 0.1f, RAND_COSMETIC);
    float y = Rand(-0.1f, 0.1f, RAND_COSMETIC);
    float x = m_direction.x;
    m_direction.y = y;
    if (x == 0.0f && y == 0.0f) {
        m_direction.x = 0.1f;
    } else {
        float length = std::sqrt(y * y + x * x);
        m_direction.x = (x / length) * 0.1f;
        m_direction.y = (m_direction.y / length) * 0.1f;
    }
    float speed = Rand(6.0f, 7.0f, RAND_COSMETIC);
    m_progress = 0.0f;
    m_speed = speed;
    m_acceleration = Rand(1.0f, 1.35f, RAND_COSMETIC);
    const GLfloat2* target = m_interface->GetTarget(m_kind == 3 ? TILE_POTION : m_type);
    float dx = target->x - m_position.x;
    float dy = target->y - m_position.y;
    m_distance = std::sqrt(dy * dy + dx * dx);
    if (tokenType == 3 || m_type != TILE_SKULL) {
        if (m_type != tile->m_type) {
            GLfloat2 tl, br;
            Tile::GetTileUV(m_type, tl, br);
            SetUV(tl, br);
        }
    } else {
        // skull tokens (XP) use the small skull icon
        SetUV(GLfloat2{0.4140625f, 0.0009765625f}, GLfloat2{0.4521484375f, 0.0224609375f});
        SetSize(GLfloat2{0.121875f, 0.071875f});
        m_speed = Rand(4.0f, 6.0f, RAND_COSMETIC);
    }
}

void Token::Process(float dt) {
    GLfloat2 target;
    if (m_target != nullptr) {
        target = m_target->m_position;
    } else if (m_kind == 1) {
        target = Interface::GetSpecialTarget(m_value);
    } else {
        target = *m_interface->GetTarget(m_kind == 3 ? TILE_POTION : m_type);
    }
    float px = m_position.x;
    float py = m_position.y;
    float dy = target.y - py;
    float dx = target.x - px;
    float distance = std::sqrt(dy * dy + dx * dx);
    float nx = dx / distance;
    float ny = dy / distance;
    float progress = m_progress + dt * m_acceleration;
    m_progress = progress;
    float step, mx, my;
    if (progress > 1.0f) {
        step = (progress * m_speed) * dt;
        mx = step * nx;
        my = step * ny;
    } else {
        step = dt * m_speed;
        float rest = 1.0f - progress;
        mx = rest * (step * m_direction.x) + progress * (step * nx);
        my = rest * (step * m_direction.y) + progress * (step * ny);
    }
    if (step > distance) {
        m_alive = false;
        return;
    }
    m_position.x = mx + px;
    m_position.y = py + my;
    float scale = Clamp(distance / m_distance, 0.2f, 1.0f);
    m_scale.x = scale;
    m_scale.y = scale;
}
