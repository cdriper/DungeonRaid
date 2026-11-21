// GLES 1.x entry points for the test library: every call is recorded as one text line
//     name arg arg ...[ |hexdata]
// Arguments are 32-bit values in hex (floats as their bit patterns). For the *Pointer calls the
// pointer argument is replaced by the bytes it points to (4 vertices; stride * 4 bytes), captured
// at call time, so traces from the port and from the original (emulator) can be compared.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "dr_gl.h"
#include "gl_record.h"

namespace {
std::string g_trace;

uint32_t Bits(float f) {
    uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

void Call(const char* name, std::initializer_list<uint32_t> args) {
    g_trace += name;
    char buf[16];
    for (uint32_t a : args) {
        std::snprintf(buf, sizeof buf, " %x", a);
        g_trace += buf;
    }
    g_trace += '\n';
}

void PointerCall(const char* name, GLint size, GLenum type, GLsizei stride, const void* p) {
    g_trace += name;
    char buf[16];
    for (uint32_t a : {static_cast<uint32_t>(size), static_cast<uint32_t>(type), static_cast<uint32_t>(stride)}) {
        std::snprintf(buf, sizeof buf, " %x", a);
        g_trace += buf;
    }
    int typeSize = (type == GL_UNSIGNED_BYTE) ? 1 : 4;
    int bytes = (stride ? stride : size * typeSize) * 4;
    g_trace += " |";
    const uint8_t* b = static_cast<const uint8_t*>(p);
    for (int i = 0; i < bytes; ++i) {
        std::snprintf(buf, sizeof buf, "%02x", b[i]);
        g_trace += buf;
    }
    g_trace += '\n';
}
}  // namespace

void GlTraceReset() { g_trace.clear(); }
const std::string& GlTrace() { return g_trace; }

extern "C" {
void sdlBindTexture(GLenum target, GLuint texture) { Call("glBindTexture", {target, texture}); }
void sdlBlendFunc(GLenum s, GLenum d) { Call("glBlendFunc", {s, d}); }
void sdlClear(GLbitfield mask) { Call("glClear", {mask}); }
void sdlClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a) { Call("glClearColor", {Bits(r), Bits(g), Bits(b), Bits(a)}); }
void sdlColorPointer(GLint size, GLenum type, GLsizei stride, const void* p) { PointerCall("glColorPointer", size, type, stride, p); }
void sdlDisable(GLenum cap) { Call("glDisable", {cap}); }
void sdlDrawArrays(GLenum mode, GLint first, GLsizei count) {
    Call("glDrawArrays", {mode, static_cast<uint32_t>(first), static_cast<uint32_t>(count)});
}
void sdlEnable(GLenum cap) { Call("glEnable", {cap}); }
void sdlEnableClientState(GLenum array) { Call("glEnableClientState", {array}); }
void sdlLoadIdentity() { Call("glLoadIdentity", {}); }
void sdlMatrixMode(GLenum mode) { Call("glMatrixMode", {mode}); }
void sdlOrthof(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f) {
    Call("glOrthof", {Bits(l), Bits(r), Bits(b), Bits(t), Bits(n), Bits(f)});
}
void sdlPopMatrix() { Call("glPopMatrix", {}); }
void sdlPushMatrix() { Call("glPushMatrix", {}); }
void sdlRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) { Call("glRotatef", {Bits(angle), Bits(x), Bits(y), Bits(z)}); }
void sdlScalef(GLfloat x, GLfloat y, GLfloat z) { Call("glScalef", {Bits(x), Bits(y), Bits(z)}); }
void sdlTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void* p) { PointerCall("glTexCoordPointer", size, type, stride, p); }
void sdlTranslatef(GLfloat x, GLfloat y, GLfloat z) { Call("glTranslatef", {Bits(x), Bits(y), Bits(z)}); }
void sdlVertexPointer(GLint size, GLenum type, GLsizei stride, const void* p) { PointerCall("glVertexPointer", size, type, stride, p); }
void sdlClipBox(GLfloat x0, GLfloat y0, GLfloat x1, GLfloat y1) { Call("portClipBox", {Bits(x0), Bits(y0), Bits(x1), Bits(y1)}); }
void sdlClipOff() { Call("portClipOff", {}); }
void sdlViewport(GLint x, GLint y, GLsizei w, GLsizei h) {
    Call("glViewport", {static_cast<uint32_t>(x), static_cast<uint32_t>(y), static_cast<uint32_t>(w), static_cast<uint32_t>(h)});
}
}
