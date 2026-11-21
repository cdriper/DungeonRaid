// The drawing calls of the game core: the original's OpenGL ES 1.x calls, renamed gl* -> sdl*
// (glDrawArrays -> sdlDrawArrays, ...), same arguments and meaning. They are declared here and
// provided by whoever links drcore:
//  - the game (platform/sdl/sdl_gl.cpp) draws them with SDL_Renderer, on every platform;
//  - the tests (tests/gl_record.cpp) record them, under the original GL names, so they can be
//    compared with the original's calls.
// The GL types and enum values are kept as in GLES 1.x.
#pragma once

#include <cstdint>

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef float GLfloat;
typedef float GLclampf;
typedef unsigned int GLbitfield;

#define GL_FLOAT 0x1406
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN 0x0006
#define GL_TEXTURE_2D 0x0DE1
#define GL_MODELVIEW 0x1700
#define GL_PROJECTION 0x1701
#define GL_BLEND 0x0BE2
#define GL_ONE 1
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_VERTEX_ARRAY 0x8074
#define GL_COLOR_ARRAY 0x8076
#define GL_TEXTURE_COORD_ARRAY 0x8078
#define GL_COLOR_BUFFER_BIT 0x4000

extern "C" {
void sdlBindTexture(GLenum target, GLuint texture);
void sdlBlendFunc(GLenum sfactor, GLenum dfactor);
void sdlClear(GLbitfield mask);
void sdlClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
void sdlColorPointer(GLint size, GLenum type, GLsizei stride, const void* pointer);
void sdlDisable(GLenum cap);
void sdlDrawArrays(GLenum mode, GLint first, GLsizei count);
void sdlEnable(GLenum cap);
void sdlEnableClientState(GLenum array);
void sdlLoadIdentity();
void sdlMatrixMode(GLenum mode);
void sdlOrthof(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat zNear, GLfloat zFar);
void sdlPopMatrix();
void sdlPushMatrix();
void sdlRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void sdlScalef(GLfloat x, GLfloat y, GLfloat z);
void sdlTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void* pointer);
void sdlTranslatef(GLfloat x, GLfloat y, GLfloat z);
void sdlVertexPointer(GLint size, GLenum type, GLsizei stride, const void* pointer);
void sdlViewport(GLint x, GLint y, GLsizei width, GLsizei height);
// Port-only (no GL counterpart; GL would use glScissor in window pixels): draw only inside the
// rectangle (x0, y0)..(x1, y1), given in the coordinates of the current model-view and projection,
// until sdlClipOff (the achievements list).
void sdlClipBox(GLfloat x0, GLfloat y0, GLfloat x1, GLfloat y1);
void sdlClipOff();
}
