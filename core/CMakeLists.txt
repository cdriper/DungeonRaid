# Reconstructed Dungeon Raid game core (portable C++17).
#
#   drcore          static library with the ported game code (src/), used by platform/ and tests/
#
# Bit-exactness: the original is ARMv7 VFP code in single precision whose multiply-adds (vmla) are
# NOT fused, so floating-point contraction must be off on every compiler.

cmake_minimum_required(VERSION 3.20)
project(DungeonRaidCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(DRCORE_SOURCES
    src/rand.cpp
    src/util.cpp
    src/game_state.cpp
    src/quad.cpp
    src/quad_strip.cpp
    src/font.cpp
    src/label.cpp
    src/simple_label.cpp
    src/particles.cpp
    src/game_tables.cpp
    src/upgrade.cpp
    src/arrow.cpp
    src/tile.cpp
    src/token.cpp
    src/skill.cpp
    src/character_class.cpp
    src/item_upgrade.cpp
    src/item.cpp
    src/menu.cpp
    src/foldout_menu.cpp
    src/game_menu.cpp
    src/game_over_screen.cpp
    src/help_menu.cpp
    src/intro_screen.cpp
    src/main_menu.cpp
    src/mode_screen.cpp
    src/name_screen.cpp
    src/class_screen.cpp
    src/score_screen.cpp
    src/unlock_menu.cpp
    src/stats_menu.cpp
    src/tooltip.cpp
    src/interface.cpp
    src/game.cpp
    src/game_actions.cpp
    src/game_match.cpp
    src/game_attack.cpp
    src/game_effects.cpp
    src/game_frame.cpp
    src/game_ui.cpp
    src/app.cpp
    src/sound.cpp
    src/settings_screen.cpp
    src/progress_code.cpp
    src/info_panel.cpp
    src/toast.cpp
    src/game_debug.cpp
    src/game_idle.cpp
    src/achievements.cpp
    src/game_achievements.cpp
    src/achievements_screen.cpp
    src/version.cpp
)
add_library(drcore STATIC ${DRCORE_SOURCES})
target_include_directories(drcore PUBLIC src)
# The version (the repo's VERSION file; the top-level CMakeLists.txt reads it), only for
# version.cpp, so a new version recompiles one file.
if(NOT DR_VERSION)
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION" DR_VERSION LIMIT_COUNT 1)
    string(STRIP "${DR_VERSION}" DR_VERSION)
endif()
set_source_files_properties(src/version.cpp PROPERTIES COMPILE_DEFINITIONS "DR_VERSION=\"${DR_VERSION}\"")
set_target_properties(drcore PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(drcore PUBLIC /fp:precise /W4 /permissive-)
    target_compile_definitions(drcore PUBLIC _CRT_SECURE_NO_WARNINGS)   # portable stdio, not fopen_s
else()
    target_compile_options(drcore PUBLIC -ffp-contract=off -Wall -Wextra)
endif()
