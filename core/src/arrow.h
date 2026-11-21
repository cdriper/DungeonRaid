// Arrow: the chain arrow drawn along the selected tiles (Game::m_arrow, 0x1684 bytes in the
// original): up to 0x120 vertices drawn as triangle fans of 4.
#pragma once

#include <list>

#include "dr_types.h"
#include "quad.h"

class Tile;

class Arrow {
public:
    static const int kMaxVertices = 0x120;

    Arrow();                                                // 0x00034e84: green (0x32, 0xff, 0x32)
    ~Arrow();                                               // 0x00034e60

    void SetColour(unsigned r, unsigned g, unsigned b);     // 0x00034e64: all vertices, alpha unchanged
    void AddVertex(const GLfloat2& position, uint8_t alpha, float u, float v);   // 0x00034eac
    void Render();                                          // 0x00034ee4
    // 0x00034f48 (the original takes the list by value): the vertices along the chain's tiles
    void Update(const std::list<Tile*>& chain);
    // Port (mouse only): Update, then a straight piece from the last tile to the pointer with the
    // head at the pointer (also for a one-tile chain). Returns false when nothing is drawn.
    bool UpdateLive(const std::list<Tile*>& chain, const GLfloat2& pointer);
    // Port (mouse only): a small round node on each chain tile, in the arrow's colour with a dark
    // rim (untextured; leaves texturing on).
    void RenderNodes(const std::list<Tile*>& chain);

    QuadVertex m_vertices[kMaxVertices];
    int m_vertexCount;
};
