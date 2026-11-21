// SimpleLabel: floating texts built from glyph rectangles of the game texture. See simple_label.h.
#include "simple_label.h"

#include "dr_gl.h"
#include "game_tables.h"
#include "rand.h"
#include "util.h"

namespace {
// Digit glyphs 0..9 (texture pixels). g_numberStarts/g_numberEnds (0x00089628/0x00089678) are
// filled by a static initialiser in the original; values read from it after .init_array.
const GLfloat2 kNumberStarts[10] = {{486, 65}, {472, 1}, {489, 1}, {425, 37}, {446, 37},
                                    {468, 37}, {489, 37}, {425, 65}, {445, 65}, {465, 65}};
const GLfloat2 kNumberEnds[10] = {{506, 92}, {488, 28}, {509, 28}, {445, 64}, {467, 64},
                                  {488, 64}, {509, 64}, {444, 92}, {464, 92}, {485, 92}};

// The word glyph of each label type (CreateTypeDefinition's switch); x0 < 0: no glyph. Types 5
// and 10 use the first rect for numbers below 2, the second otherwise ("Kill" / "Kills").
struct TypeRect {
    float x0, y0, x1, y1;
};
const TypeRect kTypeRects[42] = {
    {425, 23, 471, 36},   {425, 93, 474, 113},  {425, 93, 474, 113},  {425, 129, 499, 142},  // 0..3
    {444, 157, 484, 170}, {425, 171, 459, 184}, {425, 115, 471, 128}, {425, 115, 471, 128},  // 4..7
    {466, 143, 504, 156}, {425, 558, 465, 571}, {425, 171, 459, 184}, {425, 513, 460, 526},  // 8..11
    {461, 513, 493, 526}, {457, 527, 493, 540}, {425, 527, 456, 540}, {463, 541, 498, 556},  // 12..15
    {425, 558, 465, 571}, {466, 558, 508, 571}, {425, 541, 462, 554}, {462, 572, 494, 585},  // 16..19
    {462, 586, 502, 599}, {425, 628, 483, 641}, {425, 656, 481, 669}, {462, 614, 507, 627},  // 20..23
    {425, 670, 470, 683}, {425, 684, 479, 697}, {462, 600, 505, 613}, {425, 642, 479, 655},  // 24..27
    {425, 831, 495, 844}, {425, 845, 476, 858}, {425, 859, 481, 872}, {425, 873, 473, 886},  // 28..31
    {425, 887, 476, 900}, {425, 901, 469, 914}, {470, 901, 511, 914}, {425, 915, 487, 928},  // 32..35
    {425, 929, 481, 942}, {425, 943, 453, 956}, {425, 957, 492, 970}, {771, 425, 1023, 461}, // 36..39
    {513, 351, 817, 387}, {513, 388, 808, 424},                                              // 40..41
};
const TypeRect kPluralRect = {425, 143, 465, 156};   // types 5 and 10 with number >= 2
}  // namespace

// The original leaves everything else uninitialised; the port zeroes it.
SimpleLabel::SimpleLabel()
    : m_active(false),
      m_type(0),
      m_vertices{},
      m_glyphs{},
      m_glyphScales{},
      m_glyphCount(0),
      m_offset{0.0f, 0.0f},
      m_position{0.0f, 0.0f},
      m_scale(1.0f),
      m_numberScale(0.0f),
      m_riseSpeed(0.0f),
      m_age(0.0f),
      m_lifetime(1.0f),
      m_number(0),
      m_alpha(0xff) {}

// Rise speed: 0.1 for types 1 and 8..10, 0.08 for type 3, else random 0.1..0.2 (cosmetic stream).
void SimpleLabel::Initialise(int type, const GLfloat2& position, int number, float scale) {
    m_active = true;
    m_position = position;
    m_age = 0.0f;
    m_type = type;
    if (type == 1 || type == 8 || type == 9 || type == 10) {
        m_riseSpeed = 0.1f;
    } else if (type == 3) {
        m_riseSpeed = 0.08f;   // 0x3da3d70a
    } else {
        m_riseSpeed = Rand(0.1f, 0.2f, RAND_COSMETIC);
    }
    m_number = number;
    m_scale = scale;
    m_numberScale = GetNumberScale();
    RegenerateLabel();
}

void SimpleLabel::SetColour(int colour) {
    uint8_t r = 0xff, g = 0xff, b = 0xff;
    if (static_cast<unsigned>(colour) < 50) {
        r = g_simpleLabelColourR[colour];
        g = g_simpleLabelColourG[colour];
        b = g_simpleLabelColourB[colour];
    }
    for (QuadVertex& v : m_vertices) v.colour = GLcolour4{r, g, b, 0xff};
}

// Only labels that show a number regenerate, and only when the number changes.
void SimpleLabel::SetNumber(int number) {
    if ((UseNumberFront() || UseNumberBack()) && m_number != number) {
        m_number = number;
        RegenerateLabel();
    }
}

void SimpleLabel::SetPosition(const GLfloat2& position) { m_position = position; }

void SimpleLabel::SetAlpha(uint8_t alpha) {
    for (QuadVertex& v : m_vertices) v.colour.a = alpha;
}

// Rises at m_riseSpeed and fades from m_alpha by (1 - (age / lifetime)^2); inactive at the end.
void SimpleLabel::Process(float dt) {
    float age = dt + m_age;
    m_age = age;
    if (age >= m_lifetime) {   // (NaN keeps it alive, as the original's blt)
        m_active = false;
        return;
    }
    float t = age / m_lifetime;
    m_position.y = m_position.y - dt * m_riseSpeed;
    float f = Sqr(t);
    int count = m_glyphCount * 4;
    if (m_glyphCount <= 0) return;
    float alpha = (1.0f - f) * static_cast<float>(m_alpha);
    uint8_t a = static_cast<uint8_t>(FloatToIntArm(alpha));
    for (int i = 0; i < count; ++i) m_vertices[i].colour.a = a;
}

void SimpleLabel::Render() {
    sdlPushMatrix();
    sdlTranslatef(m_position.x, m_position.y, 0.0f);
    sdlScalef(m_scale, m_scale, m_scale);
    sdlVertexPointer(2, GL_FLOAT, sizeof(QuadVertex), &m_vertices[0].position);
    sdlColorPointer(4, GL_UNSIGNED_BYTE, sizeof(QuadVertex), &m_vertices[0].colour);
    sdlTexCoordPointer(2, GL_FLOAT, sizeof(QuadVertex), &m_vertices[0].uv);
    for (int i = 0; i < m_glyphCount; ++i) sdlDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
    sdlPopMatrix();
}

float SimpleLabel::GetNumberScale() const {
    unsigned i = static_cast<unsigned>(m_type - 3);
    return i < 27 ? g_simpleLabelNumberScales[i] : 1.0f;
}

bool SimpleLabel::UseNumberFront() const {
    unsigned i = static_cast<unsigned>(m_type - 1);
    return i < 27 && g_simpleLabelNumberFront[i] != 0;
}

bool SimpleLabel::UseNumberBack() const {
    unsigned i = static_cast<unsigned>(m_type - 3);
    return i < 27 && g_simpleLabelNumberBack[i] != 0;
}

// No bounds check (as in the original): callers keep the glyph count at most 8.
void SimpleLabel::SetDefinition(int index, const GLfloat2& topLeft, const GLfloat2& bottomRight, float scale) {
    m_glyphs[index].topLeft = topLeft;
    m_glyphs[index].bottomRight = bottomRight;
    m_glyphScales[index] = scale;
}

void SimpleLabel::CreateTypeDefinition() {
    int index = m_glyphCount;
    if (index > 7) return;
    if (static_cast<unsigned>(m_type) >= 42) return;
    TypeRect r = kTypeRects[m_type];
    if ((m_type == 5 || m_type == 10) && m_number >= 2) r = kPluralRect;
    m_glyphCount = index + 1;
    SetDefinition(index, GLfloat2{r.x0, r.y0}, GLfloat2{r.x1, r.y1}, 1.0f);
}

// Type 8 gets a second word glyph.
void SimpleLabel::CreateTypeDefinitionLast() {
    int index = m_glyphCount;
    if (index < 8 && m_type == 8) {
        m_glyphCount = index + 1;
        SetDefinition(index, GLfloat2{425.0f, 157.0f}, GLfloat2{443.0f, 170.0f}, 1.0f);
    }
}

// A minus for negative numbers, then the decimal digits without leading zeros (so 0 shows no
// digit at all). Numbers of 100000 and more (or INT_MIN) would index past the digit tables in the
// original; the game never shows such numbers.
void SimpleLabel::CreateNumberDefinition(int number) {
    if (number < 0) {
        number = -number;
        int index = m_glyphCount;
        m_glyphCount = index + 1;
        SetDefinition(index, GLfloat2{475.0f, 93.0f}, GLfloat2{488.0f, 101.0f}, 1.0f);
    }
    bool started = false;
    int divisor = 10000;
    for (int i = 0; i < 5; ++i) {
        int digit = number / divisor;
        if (digit > 0 || started) {
            int index = m_glyphCount;
            m_glyphCount = index + 1;
            started = true;
            SetDefinition(index, kNumberStarts[digit], kNumberEnds[digit], m_numberScale);
        }
        number = number % divisor;
        divisor = divisor / 10;
    }
}

// Pixel coordinates: positions / 320 (screen width units), texture coordinates / 1024.
void SimpleLabel::SetVertex(int index, float x, float y, float u, float v) {
    QuadVertex& vx = m_vertices[index];
    vx.position.x = (x + m_offset.x) / 320.0f;
    vx.position.y = (y + m_offset.y) / 320.0f;
    vx.uv.x = u * 0.0009765625f;
    vx.uv.y = v * 0.0009765625f;
}

// Lays the glyphs out left to right, centred on the position (each glyph scaled by its own
// scale, anchored at its bottom; a leading minus is lifted by its own height).
void SimpleLabel::RegenerateLabel() {
    m_glyphCount = 0;
    if (UseNumberFront()) CreateNumberDefinition(m_number);
    CreateTypeDefinition();
    if (UseNumberBack()) CreateNumberDefinition(m_number);
    CreateTypeDefinitionLast();
    int count = m_glyphCount;
    if (count <= 0) return;
    for (int i = 0; i < count * 4; ++i) m_vertices[i].colour.a = 0xff;
    float x = 0.0f;
    for (int i = 0; i < count; ++i) {
        float half = (m_glyphs[i].bottomRight.x - m_glyphs[i].topLeft.x) * -0.5f;
        x = x + half * m_glyphScales[i];
    }
    for (int i = 0;;) {
        const GlyphRect& g = m_glyphs[i];
        float s = m_glyphScales[i];
        m_offset.x = x - g.topLeft.x;
        bool minusFirst = i == 0 && m_number < 0;
        float y = -(g.topLeft.y + (g.bottomRight.y - g.topLeft.y) * s);
        m_offset.y = y;
        if (minusFirst) {
            m_offset.y = y - (m_glyphs[0].bottomRight.y - m_glyphs[0].topLeft.y) * m_glyphScales[0];
        }
        float x0 = g.topLeft.x;
        float y0 = g.topLeft.y;
        float xs = x0 + (g.bottomRight.x - x0) * s;
        float ys = y0 + s * (g.bottomRight.y - y0);
        SetVertex(i * 4, x0, y0, x0, y0);
        SetVertex(i * 4 + 1, xs, g.topLeft.y, g.bottomRight.x, g.topLeft.y);
        SetVertex(i * 4 + 2, xs, ys, g.bottomRight.x, g.bottomRight.y);
        SetVertex(i * 4 + 3, g.topLeft.x, ys, g.topLeft.x, g.bottomRight.y);
        ++i;
        if (m_glyphCount <= i) break;
        x = x + (g.bottomRight.x - g.topLeft.x) * s;
    }
}
