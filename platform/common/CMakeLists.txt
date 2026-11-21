# drhost: the platform-independent host layer (host.*: start-up, settings, saves, per-frame polling;
# media.*: PNG and Ogg decoding). Needs the stb files: cmake -P third_party/fetch.cmake

set(DR_STB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/stb")

# stb's implementations, compiled as C without warnings (and without drcore's flags).
add_library(drstb STATIC stb_impl.c)
target_include_directories(drstb PUBLIC "${DR_STB_DIR}")
if(MSVC)
    target_compile_options(drstb PRIVATE /w)
else()
    target_compile_options(drstb PRIVATE -w)
endif()

add_library(drhost STATIC host.cpp host.h media.cpp media.h)
target_include_directories(drhost PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(drhost PUBLIC drcore PRIVATE drstb)
