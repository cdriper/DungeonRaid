// Menu: base of the HUD's menus (FoldoutMenu, GameMenu, StatsMenu, Tooltip): an array of quads
// (4 vertices each) in 320-pixel screen units and 1024-pixel texture units. 8 bytes in the
// original: vtable, vertex array (allocated by the constructor, never initialised there).
#pragma once

#include "dr_types.h"
#include "quad.h"

class Menu {
public:
    explicit Menu(int quadCount);                                     // 0x0005458c
    virtual ~Menu();                                                  // 0x00054530 / 0x00054554
    Menu(const Menu&) = delete;
    Menu& operator=(const Menu&) = delete;

    void SetVertex(int index, float x, float y, float u, float v);    // 0x00054094: pixels
    void SetVertexPos(int index, float x, float y);                   // 0x000542c4
    void SetVertexUV(int index, float u, float v);                    // 0x00054218
    // quad `quad` at pos with texture rect uv/size; the size is stretched by (scaleX, scaleY) on screen
    void SetQuad(int quad, const GLfloat2& pos, const GLfloat2& uv, const GLfloat2& size, float scaleX,
                 float scaleY);                                       // 0x000540f4
    void SetQuad(int quad, const GLfloat2& uv, const GLfloat2& size); // 0x00054398: at (0, 0), same size on screen
    void SetQuad(int quad, float x, float y, float w, float h);       // 0x000544d0: position only
    void SetQuadColour(int quad, uint8_t r, uint8_t g, uint8_t b, uint8_t a);   // 0x000541dc
    void SetQuadPos(int quad, float x, float y);                      // 0x000542f8: keeps the size
    void SetQuadSize(int quad, float w, float h);                     // 0x00054450: keeps the position
    void SetQuadUV(int quad, const GLfloat2& uv, const GLfloat2& size);   // 0x00054248

    int QuadCount() const { return m_quadCount; }

    QuadVertex* m_vertices;   // 0x4: quad i = vertices 4i..4i+3 (strip order 0, 1, 2, 3 = TL, TR, BR, BL)

private:
    int m_quadCount;          // port only (the original does not keep the count)
};
