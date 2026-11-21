# Windows build

`DungeonRaid.exe` is the SDL host (`../sdl/`: main loop, input, sound, drawing on SDL_Renderer,
Direct3D 11 by default) with static SDL3 and C runtime. This folder adds what only Windows has:

| File | What |
|---|---|
| `CMakeLists.txt` | The exe from `drsdl`, the icon resource, install rules and the release zip (CPack) |
| `make_ico.cpp` | Build-time tool: packs `assets/icons/*/icon.png` into `DungeonRaid.ico` |
| `icon.rc.in` | The icon resource (SDL uses the exe's first icon) |

Build, run and package: see the top-level `README.md`.
