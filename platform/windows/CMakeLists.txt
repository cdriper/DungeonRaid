# The Windows build: DungeonRaid.exe from the SDL host (platform/sdl/, static SDL3) plus what only
# Windows has: the icon resource, the release zip.

# The icon: the original's Android icons packed into an .ico at build time, linked as a resource.
add_executable(make_ico make_ico.cpp)
set(DR_ICONS "${CMAKE_SOURCE_DIR}/assets/icons")
set(DR_ICON_FILE "${CMAKE_CURRENT_BINARY_DIR}/DungeonRaid.ico")
add_custom_command(
    OUTPUT "${DR_ICON_FILE}"
    COMMAND make_ico "${DR_ICON_FILE}" "${DR_ICONS}/ldpi/icon.png" "${DR_ICONS}/mdpi/icon.png"
            "${DR_ICONS}/hdpi/icon.png" "${DR_ICONS}/xhdpi/icon.png"
    DEPENDS make_ico "${DR_ICONS}/ldpi/icon.png" "${DR_ICONS}/mdpi/icon.png" "${DR_ICONS}/hdpi/icon.png"
            "${DR_ICONS}/xhdpi/icon.png"
    COMMENT "Making DungeonRaid.ico"
    VERBATIM)
configure_file(icon.rc.in "${CMAKE_CURRENT_BINARY_DIR}/icon.rc" @ONLY)
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/icon.rc" PROPERTIES OBJECT_DEPENDS "${DR_ICON_FILE}")

add_executable(DungeonRaid WIN32 "${CMAKE_CURRENT_BINARY_DIR}/icon.rc" "${DR_ICON_FILE}")
target_link_libraries(DungeonRaid PRIVATE drsdl)
set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT DungeonRaid)

# Release layout: DungeonRaid.exe with assets/ next to it (only what the game loads), and the SDL
# license. `cmake --build build --config Release --target package` makes the zip in build/.
install(TARGETS DungeonRaid RUNTIME DESTINATION .)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/textures" DESTINATION assets
        FILES_MATCHING PATTERN "*.png" PATTERN "splash.png" EXCLUDE)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/fonts" DESTINATION assets FILES_MATCHING PATTERN "*.fnt")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/sounds" DESTINATION assets FILES_MATCHING PATTERN "*.ogg")
install(FILES "${DR_THIRD_PARTY}/SDL3/LICENSE.txt" DESTINATION licenses RENAME SDL3.txt)

set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME DungeonRaid)
set(CPACK_PACKAGE_VERSION ${DR_VERSION})   # the repo's VERSION file
set(CPACK_PACKAGE_FILE_NAME "DungeonRaid-${DR_VERSION}-windows-x64")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_BINARY_DIR}/package")
include(CPack)
