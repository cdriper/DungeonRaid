# Golden replay test: the recorded games (data/scripts) replayed on the core, every snapshot compared
# with the original library's state (data/expected). session_driver.cpp drives the game the way the old
# emulator harness did; gl_record.cpp provides the GLES 1.x entry points off-device (records the calls).
add_executable(golden_replay golden_replay.cpp session_driver.cpp gl_record.cpp)
target_link_libraries(golden_replay PRIVATE drcore)

add_test(NAME golden_replay COMMAND golden_replay ${CMAKE_CURRENT_SOURCE_DIR}/data)

# Host smoke test: the shared host layer with the real assets and a fake platform (needs third_party/stb).
if(DR_HAVE_HOST)
    add_executable(host_smoke host_smoke.cpp gl_record.cpp)
    target_link_libraries(host_smoke PRIVATE drhost)
    add_test(NAME host_smoke COMMAND host_smoke ${CMAKE_SOURCE_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}/host_smoke_saves)
endif()

# Web build (Emscripten): ctest runs the tests with Node (the toolchain's CMAKE_CROSSCOMPILING_EMULATOR);
# NODERAWFS gives them the real files (tests/data, assets/), memory may grow (all sounds decoded).
if(EMSCRIPTEN)
    foreach(t golden_replay host_smoke)
        if(TARGET ${t})
            target_link_options(${t} PRIVATE -sNODERAWFS=1 -sALLOW_MEMORY_GROWTH=1)
        endif()
    endforeach()
endif()
