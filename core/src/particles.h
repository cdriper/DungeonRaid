// Particles: a burst of 10 shards (4 vertices each, drawn as triangle fans) that fly apart and
// fade out. Either the pieces of a collected tile (its texture cut into wedges around a jittered
// centre) or small sparks at one of an enemy's value labels. Cosmetic, but every burst draws a
// fixed number of values from the cosmetic random stream. 0x3a4 bytes in the original; the layout
// below is the original one. The constructor leaves everything uninitialised (as the original);
// the Initialise functions set every field.
#pragma once

#include "dr_types.h"
#include "quad.h"

class Tile;

class Particles {
public:
    void InitialiseFromTile(Tile* tile, int type);                      // 0x0005af68 (ParticleType): 0 tile pieces, 1 armour, 2 health, 3 attack sparks
    void InitialiseFromTile(Tile* tile);                                // 0x0005aaf8: the tile's pieces
    void InitialiseAtPosition(const GLfloat2& position, float radius);  // 0x0005a7d4: sparks
    void Process(float dt);                                             // 0x0005b02c
    void Render();                                                      // 0x0005b128

    bool m_active;                  // 0x0
    QuadVertex m_vertices[40];      // 0x4: shard i = vertices 4i..4i+3
    GLfloat2 m_velocity[10];        // 0x324
    float m_acceleration[10];       // 0x374: change of speed per second (negative)
    float m_age;                    // 0x39c
    float m_lifetime;               // 0x3a0
};

// The point where a ray from `centre` at `angle` (radians) leaves the square of half size
// `half`, with the matching texture coordinates in [u0, u1] x [v0, v1] (position and uv only).
void CastRayOntoSquare(float angle, float half, float u0, float u1, float v0, float v1, const GLfloat2& centre,
                       QuadVertex& out);   // 0x0005aa1c
