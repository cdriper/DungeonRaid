// The core's drawing calls (core/src/dr_gl.h: the original's fixed-function GLES 1.x calls, renamed
// gl* -> sdl*) implemented on SDL_Renderer, so the game draws through whatever SDL picks: Direct3D
// 11/12 on Windows, OpenGL ES 2 on Android, Metal, Vulkan, OpenGL.
// The matrix stacks live here; sdlDrawArrays transforms the vertices on the CPU and hands them to
// SDL_RenderGeometry. Only what the core uses is implemented (2D float positions, byte colours,
// float texture coordinates, triangle strips and fans, one blend function).
#pragma once

#include <SDL3/SDL.h>

namespace sdlgl {

// Draws on this renderer from now on (the gl* calls do nothing before).
void Init(SDL_Renderer* renderer);
// Creates or replaces the texture the core binds as `name` (1..6): RGBA, premultiplied.
bool SetTexture(int name, int width, int height, const void* rgba);
// Destroys the textures.
void Shutdown();

}  // namespace sdlgl
