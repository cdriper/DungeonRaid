# Android app

Gradle project for the port, on SDL3 like the desktop: SDL's `SDLActivity` (its Java sources come
from `third_party/SDL3/android-java/`, see `third_party/README.md`) starts the game's C++ `main()`
(`../sdl/main.cpp`). The NDK builds `libSDL3.so` and `libmain.so` (the core, `../common`, `../sdl`)
from the repo's top-level `CMakeLists.txt`. Drawing goes through SDL_Renderer (OpenGL ES 2); touch,
sound (SDL audio), saves and settings, the back key and immersive full screen are all in C++.

The original's textures, sounds and fonts are copied from `../../assets` into the APK's assets at
build time (`build/app/generated/sdl/`, never into `src/`), with `assets.txt` listing them; the app
copies them to its files folder on the first start of each build, because the core and the host read
files by path. The icon goes into the resources. Everything Gradle, Kotlin and the NDK generate goes
under `build/` (`build/app`, `build/cxx`, `build/.gradle`): one folder to ignore or delete.

| File | What |
|---|---|
| `app/src/main/kotlin/DungeonRaidActivity.kt` | The launcher, an `SDLActivity`: passes Android 13+ back gestures to SDL as the back key; on the first start after the Kotlin-shell version, copies its settings (SharedPreferences) into `settings.ini` |
| `app/build.gradle.kts` | SDL's Java sources, the NDK build (`main`, `SDL3-shared`), the asset copy and `assets.txt` |

Saves stay where the Kotlin shell kept them (the app's files folder, same file names and formats), so
an update keeps the game in progress, the scores and the classes.

Removed with the Kotlin shell (2026-11-12): `GameView.kt`, `Assets.kt`, `DungeonRaidJNI.kt` and
`core/src/dr_jni.cpp` (the SWIG JNI glue); they are in git history. Also gone: the splash screen with
the loading bar (the screen stays black while the sounds are decoded at start).

Versions: AGP 9.5.1, Gradle 9.6.0 (wrapper), compileSdk/targetSdk 36, minSdk 21, build-tools
36.1.0, NDK r30, CMake 3.31 (the PC's own), ABIs armeabi-v7a / arm64-v8a / x86_64. Application id
`com.fireflame.dungeonraid.recomp` (installs next to the original).

## Build (Windows, this PC)

`local.properties` (not in git) points to `C:\Android\sdk` and `C:\Program Files\CMake`. Gradle
needs JDK 17+ and the NDK build needs Ninja; both are set for the one process (no system changes):

```bash
cd platform/android
export JAVA_HOME="/c/Program Files/Java/jdk-21"
export PATH="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja:$PATH"
./gradlew assembleDebug        # -> build/app/outputs/apk/debug/app-debug.apk
```

Install on a phone with USB debugging: `C:\Android\sdk\platform-tools\adb.exe install -r build/app/outputs/apk/debug/app-debug.apk`.

## Changes from the original's behaviour

- **Back during play is ignored** (2026-11-12). With gesture navigation, a chain started on an edge tile and dragged inward is taken by the system as a back gesture, and the original's `Game::BackPressed` then opens the main menu. The host (`host::Config::ignoreBackOnBoard`, on for Android) drops back while the board is showing (no main menu, name/class/mode, intro, score or game-over screen); the in-game menu's Exit does the same as back did (`FadeInMainMenu`). Back on all screens and menus, and leaving the app from the main menu, are unchanged. The system still cancels the touch of such a swipe (the chain is dropped); `setSystemGestureExclusionRects` over the board edges would help against that (at most ~200 dp per edge), not done yet.
