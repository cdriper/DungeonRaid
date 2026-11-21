// Upgrade: base of Item, Skill and ItemUpgrade (an icon plus a name). Layout: re/structs/Upgrade.json.
// Not derived from Quad, but the icon uses Quad's vertex layout.
#pragma once

#include "dr_types.h"
#include "quad.h"

class Upgrade {
public:
    Upgrade();                                                       // 0x000672d0
    virtual ~Upgrade();                                              // 0x000673b0
    // Icon (col, row) in a texture of `textureSize` pixels with 50.03-pixel cells (49 visible),
    // shifted right by xOffset pixels.
    void SetIcon(int col, int row, int textureSize, int xOffset);    // 0x000673b8
    static void GetIconUVs(int col, int row, int textureSize, int xOffset, GLfloat2& topLeft,
                           GLfloat2& bottomRight);                   // 0x00067440
    void RenderIcon(bool extras);                                    // 0x000674bc
    virtual void RenderExtras();                                     // 0x0004bac0 (vtable slot 1)

    QuadVertex m_iconVertices[4];
    GLfloat2 m_iconPosition;
    char m_name[32];
};
