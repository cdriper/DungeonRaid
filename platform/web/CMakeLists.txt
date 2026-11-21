# The web build (Emscripten, docs/web.md): the SDL host (platform/sdl/, platform_web.cpp) as
# DungeonRaid.html + .js + .wasm + .data (the assets, preloaded into /assets) in this build folder.
# Configure with emcmake (see docs/web.md); serve the folder over HTTP (file:// does not work):
#   emrun build-web/platform/web/DungeonRaid.html
# `cmake --build build-web --target package` makes build-web/package/DungeonRaid-<version>-web.zip.

add_executable(DungeonRaid)
target_link_libraries(DungeonRaid PRIVATE drsdl)
set_target_properties(DungeonRaid PROPERTIES SUFFIX ".html")
# The assets the game loads, as in the Windows zip: the textures (not the splash), the bitmap fonts'
# .fnt files (not dwarvesfont.ttf, only the splash used it), the sounds. No icons (favicon below).
set(DR_ASSETS "${CMAKE_SOURCE_DIR}/assets")
target_link_options(DungeonRaid PRIVATE
    -sALLOW_MEMORY_GROWTH=1                  # all sounds are decoded at start
    -lidbfs.js                               # saves on IndexedDB (pre.js)
    "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/pre.js"
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
    "--preload-file=${DR_ASSETS}/textures@/assets/textures"
    "--preload-file=${DR_ASSETS}/fonts@/assets/fonts"
    "--preload-file=${DR_ASSETS}/sounds@/assets/sounds"
    "--exclude-file=*splash.png"
    "--exclude-file=*.ttf"
)
set_property(TARGET DungeonRaid APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/pre.js" "${CMAKE_CURRENT_SOURCE_DIR}/shell.html")

# The page's icon: the original's launcher icon.
add_custom_command(TARGET DungeonRaid POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DR_ASSETS}/icons/xhdpi/icon.png" "$<TARGET_FILE_DIR:DungeonRaid>/favicon.png")

# Release layout: the page as index.html (a static server opens it for the folder's address), the
# .js/.wasm/.data it loads, the favicon, the SDL license. Any static web server can host the folder
# (it must send .wasm as application/wasm, which current servers do).
set(DR_WEB_OUT "$<TARGET_FILE_DIR:DungeonRaid>")
install(FILES "${DR_WEB_OUT}/DungeonRaid.html" DESTINATION . RENAME index.html)
install(FILES "${DR_WEB_OUT}/DungeonRaid.js" "${DR_WEB_OUT}/DungeonRaid.wasm" "${DR_WEB_OUT}/DungeonRaid.data"
              "${DR_WEB_OUT}/favicon.png" DESTINATION .)
install(FILES "${DR_THIRD_PARTY}/SDL3/LICENSE.txt" DESTINATION licenses RENAME SDL3.txt)

set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME DungeonRaid)
set(CPACK_PACKAGE_VERSION ${DR_VERSION})   # the repo's VERSION file
set(CPACK_PACKAGE_FILE_NAME "DungeonRaid-${DR_VERSION}-web")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_BINARY_DIR}/package")
include(CPack)
