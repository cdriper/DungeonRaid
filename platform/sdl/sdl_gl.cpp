#include "sdl_gl.h"

#include <cmath>
#include <cstdint>
#include <vector>

#include "dr_gl.h"

// The other blend factors of GLES 1.x (the core uses only GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA).
#define GL_ZERO 0
#define GL_ONE 1
#define GL_SRC_COLOR 0x0300
#define GL_ONE_MINUS_SRC_COLOR 0x0301
#define GL_DST_ALPHA 0x0304
#define GL_ONE_MINUS_DST_ALPHA 0x0305
#define GL_DST_COLOR 0x0306
#define GL_ONE_MINUS_DST_COLOR 0x0307

namespace {

// A 4x4 matrix, column-major as in GL.
struct Mat4 {
    float m[16];
};

constexpr Mat4 kIdentity{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};

Mat4 Mul(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; c++) {
        for (int row = 0; row < 4; row++) {
            float s = 0.0f;
            for (int k = 0; k < 4; k++) s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    }
    return r;
}

struct ArrayPointer {
    GLint size = 4;
    GLenum type = GL_FLOAT;
    GLsizei stride = 0;
    const void* pointer = nullptr;
};

struct State {
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* textures[8] = {};   // by GL name; the core binds 1..6
    GLuint bound = 0;
    bool texturing = false;
    bool blending = false;
    GLenum blendSrc = GL_ONE;
    GLenum blendDst = GL_ZERO;
    bool vertexArray = false, colourArray = false, texCoordArray = false;
    ArrayPointer vertex, colour, texCoord;
    SDL_FColor clearColour{0, 0, 0, 0};
    std::vector<Mat4> modelview{kIdentity};
    std::vector<Mat4> projection{kIdentity};
    std::vector<Mat4>* current = &modelview;
    SDL_Rect viewport{0, 0, 0, 0};   // GL convention: origin bottom left
    bool clip = false;               // sdlClipBox: the box in normalised device coordinates
    float clipX0 = 0, clipY0 = 0, clipX1 = 0, clipY1 = 0;
    bool clipApplied = false;        // SDL's clip rectangle is set
    std::vector<SDL_Vertex> vertices;
    std::vector<int> indices;
} s;

Mat4& Top() { return s.current->back(); }

void MultTop(const Mat4& m) { Top() = Mul(Top(), m); }

// The GL viewport in SDL's top-left convention, against the current output height.
void ApplyViewport() {
    if (!s.renderer || s.viewport.w <= 0 || s.viewport.h <= 0) return;
    int ow = 0, oh = 0;
    SDL_GetCurrentRenderOutputSize(s.renderer, &ow, &oh);
    const SDL_Rect r{s.viewport.x, oh - (s.viewport.y + s.viewport.h), s.viewport.w, s.viewport.h};
    static SDL_Rect applied{0, 0, -1, -1};
    if (r.x == applied.x && r.y == applied.y && r.w == applied.w && r.h == applied.h) return;
    applied = r;
    SDL_SetRenderViewport(s.renderer, &r);
}

SDL_BlendFactor Factor(GLenum f) {
    switch (f) {
    case GL_ZERO: return SDL_BLENDFACTOR_ZERO;
    case GL_SRC_COLOR: return SDL_BLENDFACTOR_SRC_COLOR;
    case GL_ONE_MINUS_SRC_COLOR: return SDL_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
    case GL_SRC_ALPHA: return SDL_BLENDFACTOR_SRC_ALPHA;
    case GL_ONE_MINUS_SRC_ALPHA: return SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    case GL_DST_ALPHA: return SDL_BLENDFACTOR_DST_ALPHA;
    case GL_ONE_MINUS_DST_ALPHA: return SDL_BLENDFACTOR_ONE_MINUS_DST_ALPHA;
    case GL_DST_COLOR: return SDL_BLENDFACTOR_DST_COLOR;
    case GL_ONE_MINUS_DST_COLOR: return SDL_BLENDFACTOR_ONE_MINUS_DST_COLOR;
    default: return SDL_BLENDFACTOR_ONE;
    }
}

SDL_BlendMode CurrentBlendMode() {
    if (!s.blending) return SDL_BLENDMODE_NONE;
    if (s.blendSrc == GL_SRC_ALPHA && s.blendDst == GL_ONE_MINUS_SRC_ALPHA) return SDL_BLENDMODE_BLEND;   // the core's
    const SDL_BlendFactor src = Factor(s.blendSrc), dst = Factor(s.blendDst);
    return SDL_ComposeCustomBlendMode(src, dst, SDL_BLENDOPERATION_ADD, src, dst, SDL_BLENDOPERATION_ADD);
}

const uint8_t* Element(const ArrayPointer& a, int i, int naturalSize) {
    const int stride = a.stride ? a.stride : naturalSize;
    return static_cast<const uint8_t*>(a.pointer) + static_cast<size_t>(i) * stride;
}

}  // namespace

namespace sdlgl {

void Init(SDL_Renderer* renderer) {
    // The texture address mode stays SDL's default (AUTO): it clamps whenever the coordinates are
    // inside [0, 1], which the core's always are, so it matches the Android shell's CLAMP_TO_EDGE.
    // An explicit CLAMP would make the software renderer turn quads into blits, which gets rotated
    // ones wrong.
    s.renderer = renderer;
}

bool SetTexture(int name, int width, int height, const void* rgba) {
    if (!s.renderer || name <= 0 || name >= static_cast<int>(SDL_arraysize(s.textures))) return false;
    if (s.textures[name]) SDL_DestroyTexture(s.textures[name]);
    SDL_Texture* t = SDL_CreateTexture(s.renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
    s.textures[name] = t;
    if (!t) return false;
    // One filter for both directions in SDL; GL had nearest for minification, linear for
    // magnification. Linear keeps the shrunk glyphs and icons smooth.
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
    return SDL_UpdateTexture(t, nullptr, rgba, width * 4);
}

void Shutdown() {
    for (SDL_Texture*& t : s.textures) {
        if (t) SDL_DestroyTexture(t);
        t = nullptr;
    }
    s.renderer = nullptr;
}

}  // namespace sdlgl

// ---------------------------------------------- the core's drawing calls (GLES 1.x, renamed sdl*)

extern "C" {

void sdlBindTexture(GLenum /*target*/, GLuint texture) { s.bound = texture; }

void sdlBlendFunc(GLenum sfactor, GLenum dfactor) {
    s.blendSrc = sfactor;
    s.blendDst = dfactor;
}

void sdlClear(GLbitfield mask) {
    if (!s.renderer || !(mask & GL_COLOR_BUFFER_BIT)) return;
    // As glClear: the whole target, whatever the viewport.
    SDL_SetRenderDrawColorFloat(s.renderer, s.clearColour.r, s.clearColour.g, s.clearColour.b, s.clearColour.a);
    SDL_RenderClear(s.renderer);
}

void sdlClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a) { s.clearColour = SDL_FColor{r, g, b, a}; }

void sdlColorPointer(GLint size, GLenum type, GLsizei stride, const void* pointer) {
    s.colour = ArrayPointer{size, type, stride, pointer};
}

void sdlTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void* pointer) {
    s.texCoord = ArrayPointer{size, type, stride, pointer};
}

void sdlVertexPointer(GLint size, GLenum type, GLsizei stride, const void* pointer) {
    s.vertex = ArrayPointer{size, type, stride, pointer};
}

void sdlEnable(GLenum cap) {
    if (cap == GL_TEXTURE_2D) s.texturing = true;
    else if (cap == GL_BLEND) s.blending = true;
}

void sdlDisable(GLenum cap) {
    if (cap == GL_TEXTURE_2D) s.texturing = false;
    else if (cap == GL_BLEND) s.blending = false;
}

void sdlEnableClientState(GLenum array) {
    if (array == GL_VERTEX_ARRAY) s.vertexArray = true;
    else if (array == GL_COLOR_ARRAY) s.colourArray = true;
    else if (array == GL_TEXTURE_COORD_ARRAY) s.texCoordArray = true;
}

void sdlMatrixMode(GLenum mode) { s.current = (mode == GL_PROJECTION) ? &s.projection : &s.modelview; }

void sdlLoadIdentity() { Top() = kIdentity; }

void sdlPushMatrix() { s.current->push_back(s.current->back()); }

void sdlPopMatrix() {
    if (s.current->size() > 1) s.current->pop_back();
}

void sdlOrthof(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f) {
    Mat4 m = kIdentity;
    m.m[0] = 2.0f / (r - l);
    m.m[5] = 2.0f / (t - b);
    m.m[10] = -2.0f / (f - n);
    m.m[12] = -(r + l) / (r - l);
    m.m[13] = -(t + b) / (t - b);
    m.m[14] = -(f + n) / (f - n);
    MultTop(m);
}

void sdlTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    Mat4 m = kIdentity;
    m.m[12] = x;
    m.m[13] = y;
    m.m[14] = z;
    MultTop(m);
}

void sdlScalef(GLfloat x, GLfloat y, GLfloat z) {
    Mat4 m = kIdentity;
    m.m[0] = x;
    m.m[5] = y;
    m.m[10] = z;
    MultTop(m);
}

void sdlRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    if (angle == 0.0f) return;   // the core rotates almost every quad by 0
    const float len = std::sqrt(x * x + y * y + z * z);
    if (len == 0.0f) return;
    x /= len;
    y /= len;
    z /= len;
    const float rad = angle * 3.14159265358979f / 180.0f;
    const float c = std::cos(rad), sn = std::sin(rad), ic = 1.0f - c;
    Mat4 m = kIdentity;
    m.m[0] = x * x * ic + c;
    m.m[1] = y * x * ic + z * sn;
    m.m[2] = x * z * ic - y * sn;
    m.m[4] = x * y * ic - z * sn;
    m.m[5] = y * y * ic + c;
    m.m[6] = y * z * ic + x * sn;
    m.m[8] = x * z * ic + y * sn;
    m.m[9] = y * z * ic - x * sn;
    m.m[10] = z * z * ic + c;
    MultTop(m);
}

void sdlClipBox(GLfloat x0, GLfloat y0, GLfloat x1, GLfloat y1) {
    // the corners through the current matrices to normalised device coordinates (kept, so the box
    // follows the viewport it is drawn in)
    const Mat4 mvp = Mul(s.projection.back(), s.modelview.back());
    const float* m = mvp.m;
    auto ndc = [m](float x, float y, float& nx, float& ny) {
        nx = m[0] * x + m[4] * y + m[12];
        ny = m[1] * x + m[5] * y + m[13];
    };
    ndc(x0, y0, s.clipX0, s.clipY0);
    ndc(x1, y1, s.clipX1, s.clipY1);
    s.clip = true;
}

void sdlClipOff() {
    s.clip = false;
    if (s.clipApplied && s.renderer) SDL_SetRenderClipRect(s.renderer, nullptr);
    s.clipApplied = false;
}

void sdlViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    s.viewport = SDL_Rect{x, y, width, height};
    ApplyViewport();
}

void sdlDrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (!s.renderer || !s.vertexArray || !s.vertex.pointer || count < 3) return;
    if (mode != GL_TRIANGLE_STRIP && mode != GL_TRIANGLE_FAN) return;   // all the core draws
    ApplyViewport();   // the output may have changed size since glViewport
    if (s.clip) {      // sdlClipBox: in viewport pixels (y down), as the vertices below
        const float vw0 = static_cast<float>(s.viewport.w), vh0 = static_cast<float>(s.viewport.h);
        const float ax = (s.clipX0 * 0.5f + 0.5f) * vw0, bx = (s.clipX1 * 0.5f + 0.5f) * vw0;
        const float ay = (0.5f - s.clipY0 * 0.5f) * vh0, by = (0.5f - s.clipY1 * 0.5f) * vh0;
        const int l = static_cast<int>(std::floor(ax < bx ? ax : bx)), r = static_cast<int>(std::ceil(ax < bx ? bx : ax));
        const int t = static_cast<int>(std::floor(ay < by ? ay : by)), b = static_cast<int>(std::ceil(ay < by ? by : ay));
        const SDL_Rect rect{l, t, r - l, b - t};
        SDL_SetRenderClipRect(s.renderer, &rect);
        s.clipApplied = true;
    }

    const Mat4 mvp = Mul(s.projection.back(), s.modelview.back());
    const float vw = static_cast<float>(s.viewport.w), vh = static_cast<float>(s.viewport.h);
    const bool textured = s.texturing && s.texCoordArray && s.texCoord.pointer && s.bound > 0 &&
                          s.bound < SDL_arraysize(s.textures) && s.textures[s.bound];
    const bool coloured = s.colourArray && s.colour.pointer;

    s.vertices.resize(static_cast<size_t>(count));
    for (int i = 0; i < count; i++) {
        const float* p = reinterpret_cast<const float*>(Element(s.vertex, first + i, s.vertex.size * 4));
        const float px = p[0], py = s.vertex.size > 1 ? p[1] : 0.0f, pz = s.vertex.size > 2 ? p[2] : 0.0f;
        const float* m = mvp.m;
        float cx = m[0] * px + m[4] * py + m[8] * pz + m[12];
        float cy = m[1] * px + m[5] * py + m[9] * pz + m[13];
        const float cw = m[3] * px + m[7] * py + m[11] * pz + m[15];
        if (cw != 0.0f && cw != 1.0f) {
            cx /= cw;
            cy /= cw;
        }
        SDL_Vertex& v = s.vertices[static_cast<size_t>(i)];
        // Normalised device coordinates to viewport pixels (y down).
        v.position = SDL_FPoint{(cx * 0.5f + 0.5f) * vw, (0.5f - cy * 0.5f) * vh};
        if (coloured) {
            if (s.colour.type == GL_UNSIGNED_BYTE) {
                const uint8_t* c = Element(s.colour, first + i, s.colour.size);
                v.color = SDL_FColor{c[0] / 255.0f, c[1] / 255.0f, c[2] / 255.0f, c[3] / 255.0f};
            } else {
                const float* c = reinterpret_cast<const float*>(Element(s.colour, first + i, s.colour.size * 4));
                v.color = SDL_FColor{c[0], c[1], c[2], c[3]};
            }
        } else {
            v.color = SDL_FColor{1, 1, 1, 1};   // GL's initial current colour (the core never sets one)
        }
        if (textured) {
            const float* t = reinterpret_cast<const float*>(Element(s.texCoord, first + i, s.texCoord.size * 4));
            v.tex_coord = SDL_FPoint{t[0], t[1]};
        } else {
            v.tex_coord = SDL_FPoint{0, 0};
        }
    }

    s.indices.clear();
    for (int i = 0; i + 2 < count; i++) {
        if (mode == GL_TRIANGLE_FAN) {
            s.indices.insert(s.indices.end(), {0, i + 1, i + 2});
        } else {
            s.indices.insert(s.indices.end(), {i, i + 1, i + 2});
        }
    }

    const SDL_BlendMode blend = CurrentBlendMode();
    SDL_Texture* texture = textured ? s.textures[s.bound] : nullptr;
    if (texture) SDL_SetTextureBlendMode(texture, blend);
    else SDL_SetRenderDrawBlendMode(s.renderer, blend);
    SDL_RenderGeometry(s.renderer, texture, s.vertices.data(), count, s.indices.data(),
                       static_cast<int>(s.indices.size()));
}

}  // extern "C"
