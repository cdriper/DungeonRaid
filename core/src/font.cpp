// Font, ported from the disassembly. Divisions convert each int separately (vcvt) and every
// multiply-add is unfused, as in the original.
#include "font.h"

#include <cstdio>

#include "util.h"

namespace {
int Glyph(char c) {
    return ClampI(static_cast<unsigned char>(c), 0, Font::kMaxCharacters - 1);
}

// bionic _toupper_tab_ (ASCII only; other codes map to themselves)
int ToUpper(int c) {
    return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c;
}

// Same arithmetic in LoadFont and AdjustCharacter.
void SetGlyph(FontGlyph& g, int lineHeight, int base, int x, int y, int width, int height,
              int scaleW, int scaleH, int xoffset, int yoffset, int xadvance) {
    float sw = static_cast<float>(scaleW);
    float sh = static_cast<float>(scaleH);
    float lh = static_cast<float>(lineHeight);
    float u0 = static_cast<float>(x) / sw;
    float v0 = static_cast<float>(y) / sh;
    g.topLeft = {u0, v0};
    g.bottomRight = {u0 + static_cast<float>(width) / sw, v0 + static_cast<float>(height) / sh};
    g.size = {static_cast<float>(width) / lh, static_cast<float>(height) / lh};
    g.offset.x = (static_cast<float>(xoffset) / lh) * 0.5f;
    float halfHeight = g.size.y * 0.5f;
    float oy = static_cast<float>(yoffset) / lh + halfHeight;
    g.offset.y = oy - static_cast<float>(base) / lh;
    g.advance = (static_cast<float>(xadvance) / lh) * 0.5f;
}
}  // namespace

// Texture, line height and base stay uninitialised in the original; the port zeroes them.
Font::Font() : m_glyphs{}, m_texture(0), m_smallCaps(-1.0f), m_spacing(0.0f), m_lineHeight(0), m_base(0) {}

Font::Font(const Font& other) = default;

Font::~Font() = default;

// AngelCode text format, read with fgets(256) + sscanf like the original (so a line longer than
// 255 characters is split across two reads). Binary mode: bionic's "r" doesn't translate CRLF.
void Font::LoadFont(const char* path, int texture) {
    m_texture = texture;
    std::FILE* f = std::fopen(path, "rb");
    if (!f) {
        return;
    }
    char line[256];
    int scaleW = 0, scaleH = 0, count = 0;
    std::fgets(line, sizeof line, f);
    std::fgets(line, sizeof line, f);
    std::sscanf(line, "common lineHeight=%d base=%d scaleW=%d scaleH=%d", &m_lineHeight, &m_base, &scaleW, &scaleH);
    std::fgets(line, sizeof line, f);
    std::fgets(line, sizeof line, f);
    std::sscanf(line, "chars count=%d", &count);
    for (int i = 0; i < count; ++i) {
        unsigned id = 0;
        int x = 0, y = 0, w = 0, h = 0, xo = 0, yo = 0, xa = 0;
        std::fgets(line, sizeof line, f);
        std::sscanf(line, "char id=%d x=%d y=%d width=%d height=%d xoffset=%d yoffset=%d xadvance=%d",
                    reinterpret_cast<int*>(&id), &x, &y, &w, &h, &xo, &yo, &xa);
        if (id < static_cast<unsigned>(kMaxCharacters)) {
            SetGlyph(m_glyphs[id], m_lineHeight, m_base, x, y, w, h, scaleW, scaleH, xo, yo, xa);
        }
    }
    std::fclose(f);
}

void Font::AdjustCharacter(int id, int x, int y, int width, int height, int scaleW, int scaleH,
                           int xoffset, int yoffset, int xadvance) {
    SetGlyph(m_glyphs[id], m_lineHeight, m_base, x, y, width, height, scaleW, scaleH, xoffset, yoffset, xadvance);
}

// Lower-case glyphs become scaled copies of the upper-case ones. A negative scale only stores it.
void Font::UseSmallCaps(float scale) {
    m_smallCaps = scale;
    if (scale < 0.0f) {
        return;
    }
    for (int c = 0; c < kMaxCharacters; ++c) {
        if (c >= 'a' && c <= 'z') {
            const FontGlyph& up = m_glyphs[ToUpper(c)];
            FontGlyph& g = m_glyphs[c];
            g.topLeft = up.topLeft;
            g.bottomRight = up.bottomRight;
            g.size = {m_smallCaps * up.size.x, m_smallCaps * up.size.y};
            g.offset = {m_smallCaps * up.offset.x, m_smallCaps * up.offset.y};
            g.advance = up.advance * m_smallCaps;
        }
    }
}

// Space: spacing + spaceScale * advance; anything else: advance + spacing.
float Font::GetAdvance(char c, float spaceScale) const {
    if (static_cast<unsigned char>(c) == ' ') {
        float scaled = spaceScale * m_glyphs[Glyph(' ')].advance;
        return m_spacing + scaled;
    }
    return m_glyphs[Glyph(c)].advance + m_spacing;
}

const GLfloat2& Font::GetTopLeft(char c) const {
    int code = static_cast<unsigned char>(c);
    if (0.0f < m_smallCaps) {
        code = ToUpper(code);
    }
    return m_glyphs[ClampI(code, 0, kMaxCharacters - 1)].topLeft;
}

const GLfloat2& Font::GetBottomRight(char c) const {
    int code = static_cast<unsigned char>(c);
    if (0.0f < m_smallCaps) {
        code = ToUpper(code);
    }
    return m_glyphs[ClampI(code, 0, kMaxCharacters - 1)].bottomRight;
}

const GLfloat2& Font::GetSize(char c) const {
    return m_glyphs[Glyph(c)].size;
}

const GLfloat2& Font::GetOffset(char c) const {
    return m_glyphs[Glyph(c)].offset;
}

// Greedy word wrap. Each character (spaces included) measures size.x / 2 + GetAdvance(c, 2);
// when the running total exceeds width / size, the text up to the last space becomes a line and
// measuring restarts after that space. The terminating NUL adds 1000, which flushes the last line
// (unless width / size >= 1000). The original crashes on a word wider than the line (memcpy with
// a negative length); the port pushes an empty line there instead.
void Font::SplitLines(const char* text, float size, float width, std::vector<std::string>& lines) const {
    float limit = width / size;
    float measured = 0.0f;
    int lineStart = 0;
    int lastBreak = 0;
    int i = 0;
    for (;;) {
        char c = text[i];
        if (c == ' ' || c != '\0') {
            if (c == ' ') {
                lastBreak = i;
            }
            float half = GetSize(c).x * 0.5f;
            float step = half + GetAdvance(c, 2.0f);
            measured = measured + step;
        } else {
            lastBreak = i;
            measured = 1000.0f;
        }
        int at = i;
        if (measured > limit) {
            int length = lastBreak - lineStart;
            lines.emplace_back(length > 0 ? std::string(text + lineStart, static_cast<size_t>(length)) : std::string());
            lineStart = lastBreak + 1;
            i = lastBreak;
            at = lastBreak;
            measured = 0.0f;
        }
        if (text[at] == '\0') {
            return;
        }
        ++i;
    }
}
