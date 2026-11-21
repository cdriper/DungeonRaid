# Dungeon Raid

**Dungeon Raid** (2012, Fireflame Games) is a tile-matching dungeon crawler: drag chains of skulls,
swords, shields, potions and coins, level up your hero, buy gear and see how deep you can go. It
was published on iOS and Android and has been delisted from every store for over ten years. This
repository brings it back, so it can still be played on today's devices.

## About this project

- **Preservation.** The goal is to keep the game playable. The game logic was recompiled into
  portable C++17 from the original Android release (`com.fireflame.dungeonraid` v1.2.11, its ARM32
  native library), function by function.
- **The original gameplay, unchanged.** The game state is bit-exact with the original: a golden
  replay test plays recorded games of the original on the new core and compares every snapshot.
  Rules, numbers, enemies, items, skills, classes and modes are those of 2012.
- **Quality-of-life features**, all optional or on top of the original game:
  - a Settings screen (volume, help pages, clock, attack dimming, grind speed, fullscreen);
  - an info panel that previews what a chain will do (kills, XP, health, armour, gold, the
    enemies' next attack) and describes the tile or skill under the cursor; layouts for tall,
    wide and desktop screens;
  - an Achievements screen (the original kept them on OpenFeint, which is gone);
  - progress export/import through a progress code, and progress reset;
  - dates in the high score tables;
  - mouse and keyboard controls on desktop, including an alternative click-to-select mode.
- **Migrated to SDL3.** The original Java shell and OpenGL ES 1.x calls were replaced by a single
  SDL3 host (SDL_Renderer for drawing, SDL audio and input), shared by all platforms.
- **Ports for Android, Windows and the Web** (Emscripten/WebAssembly; desktop and mobile
  browsers). Saves use the same format everywhere, so progress can move between them.

## Web version

Web version:  [https://cdriper.github.io/DungeonRaid/](https://cdriper.github.io/DungeonRaid/)

## Copyright

Dungeon Raid — the game, its design, art, sounds and music — © Fireflame Games / Alex Kuptsov.

See [`LICENSE`](LICENSE): the source code is under the MIT License; the game assets (`assets/`)
and the name are all rights reserved, free to play and redistribute unmodified with this
project for non-commercial use.

## Credits

- **Fireflame Games / Alex Kuptsov** — the original game.
- **Claude** (Anthropic), through Claude Code — helped a great deal with this project: the reverse
  engineering and recompilation of the original library, the differential testing against it, the
  SDL3 host, the Android, Windows and Web ports, and the documentation.
- [SDL](https://www.libsdl.org/) (zlib license) and [stb](https://github.com/nothings/stb)
  (public domain / MIT).

## Repository layout

| Folder | Contents |
|---|---|
| `core/` | The game: portable C++17, CMake (`drcore`). Comments with `0x...` addresses point into the original library (Ghidra image base `0x10000`). Porting notes and accepted differences: `core/README.md`. |
| `platform/common/` | Shared C++ host layer (`drhost`): start-up, settings, saves, the per-frame polling, PNG/Ogg decoding. |
| `platform/sdl/` | The SDL3 host, one for all platforms: main loop, touch/mouse/keyboard, sound (`main.cpp`); what differs per platform behind `platform.h` (`platform_windows.cpp`, `platform_android.cpp`, `platform_web.cpp`, e.g. Android's asset extraction, the web's browser-driven loop; no `#ifdef`s); the core's drawing calls (`sdl*`, the original's GLES 1.x calls renamed) on SDL_Renderer (`sdl_gl.cpp`). |
| `platform/windows/` | Windows: `DungeonRaid.exe` from `platform/sdl/` (Direct3D 11 by default), the icon, the release zip. |
| `platform/android/` | Gradle project: SDL's `SDLActivity` + a small Kotlin launcher; the NDK builds `libSDL3.so` and `libmain.so` (the core + `platform/sdl/`) from this repo's `CMakeLists.txt`. |
| `platform/web/` | The web build (Emscripten): `DungeonRaid.html/.js/.wasm/.data` from `platform/sdl/` (`platform_web.cpp`), the page template (`shell.html`), the saves on IndexedDB (`pre.js`), the release zip. |
| `assets/` | The original game's textures, sounds, fonts and app icons. `textures/tiles.png` also holds three port-only images made from the original buttons' own letters: the title screen's "Settings" (at 912,200, 107×42), the pause menu's Settings button (at 820,350, 89×26) and its Debug button (at 912,350, 89×26; debug mode only; the "b" is hand-drawn in the same style); and two title-screen buttons drawn in the buttons' own font (`fonts/dwarvesfont.ttf`) and style: "Achievements" (at 824,380, 192×40) and "Exit" (at 730,466, 77×40; desktop only). |
| `third_party/` | SDL3 and stb, downloaded by `third_party/fetch.cmake` (not in git). |
| `tests/` | Golden replay test (8 recorded games replayed on the core, every snapshot compared with the original's state) and the host smoke test. |
| `docs/` | `manual.md`: the player's manual (rules, items, skills, enemies, classes, modes). `roadmap.md`: multi-platform plan. `web.md`: the web port (Emscripten): plan, phases, results. `tools.md`: tools installed on the dev PC and how to remove them. |

## Build

Desktop (core, Windows host, tests; Visual Studio 2022). The first line downloads the third-party
sources once; without them only the core and the golden test are built:

```
cmake -P third_party/fetch.cmake
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The game: `build/platform/windows/Release/DungeonRaid.exe` (finds the repo's `assets/` in a
development build). Mouse = touch, Esc = back (on the board it opens and closes the pause menu;
it quits from the main menu), Alt+Enter or F11 =
fullscreen (also the Fullscreen setting, saved; off at first); on the name screen the keyboard types while the on-screen keyboard is up (Backspace,
Enter = OK). Saves and settings: `%APPDATA%\DungeonRaid\` (same files as on Android). Another
render backend: set `SDL_RENDER_DRIVER` (`direct3d12`, `opengl`, `vulkan`) before starting it.
Debug mode: click the build info (bottom right of the title screen) three times; then Ctrl+S adds
a random skill during a game, Ctrl+A plays an idle animation, and Settings shows a Debug mode
tick box to turn it off.

Release zip (`DungeonRaid.exe`, `assets/` with only what the game loads, the SDL license) in
`build/package/`:

```
cmake --build build --config Release --target package
```

Android (Git Bash; needs the third-party sources as above and `platform/android/local.properties`
with `sdk.dir` and `cmake.dir`):

```
cd platform/android
export JAVA_HOME="/c/Program Files/Java/jdk-21"
export PATH="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja:$PATH"
./gradlew assembleDebug        # -> build/app/outputs/apk/debug/app-debug.apk
```

Web (PowerShell; needs the third-party sources as above and the Emscripten SDK in `C:\tools\emsdk`,
see `docs/tools.md`; Ninja from Visual Studio). Details and the port's history: `docs/web.md`.

```
C:\tools\emsdk\emsdk_env.ps1
$env:PATH = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;" + $env:PATH
emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-web
ctest --test-dir build-web --output-on-failure            # the tests under Node
emrun --no_browser --port 6931 build-web\platform\web      # then http://localhost:6931/DungeonRaid.html
cmake --build build-web --target package                  # -> build-web/package/DungeonRaid-<version>-web.zip
```

The page must come from a web server (`file://` does not work). The zip (`index.html`, `.js`,
`.wasm`, `.data` with only the assets the game loads, the favicon, the SDL license) can be put on any
static web server. Saves and settings live in the browser's IndexedDB for that site (the same files
as on the desktop; another address or port has its own saves; clearing the site data deletes them;
the Settings' progress Export/Import moves the classes). No Exit button and Esc does not quit on the
main menu; F5 / Ctrl+R / F12 stay the browser's. Mobile browsers: touch works as on Android.

Keep paths shallow (≤ 7 folder levels below the repo root): Windows path length and the assistant's file access.
