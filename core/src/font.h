// Font: a bitmap font of 128 glyphs. On Android the Java shell parses the AngelCode .fnt files
// (res/raw/font*.fnt) and pushes each glyph in with AdjustCharacter; LoadFont parses them natively
// (not used by the original Android build, useful for the desktop host).
//
// Characters are unsigned (ARM's plain char): codes 128..255 map to glyph 127.
// The layout is kept exactly (0x1214 bytes, 32/64-bit independent) so tests can copy fonts
// between the port and the original.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "dr_types.h"

struct FontGlyph {
    GLfloat2 topLeft;       // texture coordinates
    GLfloat2 bottomRight;
    GLfloat2 size;          // in line heights
    GLfloat2 offset;        // centre offset, in line heights
    float advance;          // half the xadvance, in line heights
};

class Font {
public:
    static const int kMaxCharacters = 128;

    Font();                          // 0x00043edc
    Font(const Font& other);         // 0x00043830
    Font& operator=(const Font& other) = default;
    ~Font();                         // 0x00043828

    void LoadFont(const char* path, int texture);                                   // 0x00043c40
    void AdjustCharacter(int id, int x, int y, int width, int height, int scaleW, int scaleH,
                         int xoffset, int yoffset, int xadvance);                   // 0x00043980
    void UseSmallCaps(float scale);                                                 // 0x000438bc
    float GetAdvance(char c, float spaceScale) const;                               // 0x00043b30
    const GLfloat2& GetTopLeft(char c) const;                                       // 0x00043c04
    const GLfloat2& GetBottomRight(char c) const;                                   // 0x00043bc4
    const GLfloat2& GetSize(char c) const;                                          // 0x00043ba8
    const GLfloat2& GetOffset(char c) const;                                        // 0x00043b8c
    void SplitLines(const char* text, float size, float width, std::vector<std::string>& lines) const;  // 0x000440c4

    // Inlined by SWIG into the JNI stubs (Java_..._Font_1*):
    int GetBase() const { return m_base; }
    void SetBase(int base) { m_base = base; }
    int GetLineHeight() const { return m_lineHeight; }
    void SetLineHeight(int lineHeight) { m_lineHeight = lineHeight; }
    int GetTexture() const { return m_texture; }
    void SetTexture(int texture) { m_texture = texture; }
    static int GetMaxNumCharacters() { return kMaxCharacters; }

    FontGlyph m_glyphs[kMaxCharacters];   // +0x0
    int m_texture;                        // +0x1200 (Texture)
    float m_smallCaps;                    // +0x1204; > 0: lower case is drawn with the upper-case glyphs
    float m_spacing;                      // +0x1208; added to every advance
    int m_lineHeight;                     // +0x120c
    int m_base;                           // +0x1210
};
