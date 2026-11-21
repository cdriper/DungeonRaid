# Third-party code

Pinned upstream sources. They are **not stored in git**: after cloning, download them once from the
repo root (needs only CMake, any platform):

```
cmake -P third_party/fetch.cmake
```

The script downloads each file, checks its SHA-256 (it stops on a mismatch) and puts it here. Run it
again any time: what is already up to date is skipped (`-DFORCE=ON` before `-P` downloads again).
Don't edit the downloaded sources; to update a library, change its version, URL and checksum in
`fetch.cmake`, run it with `-DFORCE=ON`, and update this table and `docs/tools.md`.

| Folder | Version | Source | License |
|---|---|---|---|
| `SDL3/` | 3.4.20 (`release-3.4.20-0-gf00dc919c`) | https://github.com/libsdl-org/SDL/releases/tag/release-3.4.20 (`SDL3-3.4.20.tar.gz`, SHA-256 `f3bb06f5…8581f`) | zlib (`SDL3/LICENSE.txt`) |
| `stb/stb_image.h` | v2.31, commit `78770d85` | https://github.com/nothings/stb | public domain / MIT (end of file) |
| `stb/stb_vorbis.c` | v1.23, commit `78770d85` | https://github.com/nothings/stb | public domain / MIT (end of file) |

SDL3 is trimmed to what its CMake build needs: `CMakeLists.txt`, `cmake/`, `include/`, `src/`,
`wayland-protocols/` and the license/readme files (33 MB). Left out: `test/`, `examples/`, `docs/`,
`VisualC*/`, `Xcode/`, `build-scripts/`, the rest of `android-project/` and `src/hidapi/testgui/`
(too deep for the repo's 7-level path limit). SDL's Android Java sources (`SDLActivity` and the
rest of package `org.libsdl.app`, from `android-project/app/src/main/java/org/libsdl/app/`) are
copied flat into `SDL3/android-java/`; the Android app compiles them. Configure it with
`SDL_TESTS=OFF`, `SDL_TEST_LIBRARY=OFF` and `SDL_EXAMPLES=OFF`.
