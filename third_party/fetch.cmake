# Downloads the pinned third-party sources into third_party/ (ignored by git) and verifies their
# SHA-256. Needs only CMake (any platform), run from the repo root:
#
#   cmake -P third_party/fetch.cmake                  # skips what is already up to date
#   cmake -DFORCE=ON -P third_party/fetch.cmake       # download again
#   cmake -DDEST=<dir> -P third_party/fetch.cmake     # into another folder (e.g. to check the result)
#
# (-D options go before -P.) To update a library: change its version, URL and checksum here, run
# with FORCE, and update third_party/README.md and docs/tools.md.
cmake_minimum_required(VERSION 3.20)

set(SDL_VERSION 3.4.20)
set(SDL_URL "https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL3-${SDL_VERSION}.tar.gz")
set(SDL_SHA256 f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f)
# What the CMake build needs; tests, examples, docs and IDE projects are left out.
set(SDL_KEEP CMakeLists.txt cmake include src wayland-protocols LICENSE.txt README.md REVISION.txt CREDITS.md WhatsNew.txt)
# Not built, and deeper than the repo's 7-level path rule.
set(SDL_DROP src/hidapi/testgui)
# SDL's Android Java sources (SDLActivity, ...): package org.libsdl.app, copied flat into
# SDL3/android-java/ (their own folder is 9 levels deep); the Android app compiles them.
set(SDL_ANDROID_JAVA android-project/app/src/main/java/org/libsdl/app)

set(STB_COMMIT 78770d85c4d94787a56c7859017da963bc4f3439)
set(STB_FILES stb_image.h stb_vorbis.c)
set(STB_SHA256_stb_image.h 0e266e9f94592d9b87e97fa801a6d7e1bf8ebfe5809e5d7838f78257cba46e0c)
set(STB_SHA256_stb_vorbis.c 7399fe71d37b84148b6a6742f4116313bd47492824e949ce7f3cdcc8cee13693)

if(NOT DEFINED DEST)
    set(DEST "${CMAKE_CURRENT_LIST_DIR}")
endif()
get_filename_component(DEST "${DEST}" ABSOLUTE)

if(DEFINED ENV{TEMP})
    set(tmp "$ENV{TEMP}/dungeonraid-fetch")
else()
    set(tmp "/tmp/dungeonraid-fetch")
endif()
file(REMOVE_RECURSE "${tmp}")
file(MAKE_DIRECTORY "${tmp}" "${DEST}")

function(fetch url file sha256)
    message(STATUS "  downloading ${url}")
    file(DOWNLOAD "${url}" "${file}" TLS_VERIFY ON EXPECTED_HASH SHA256=${sha256} STATUS st)
    list(GET st 0 code)
    if(NOT code EQUAL 0)
        list(GET st 1 msg)
        file(REMOVE_RECURSE "${tmp}")
        message(FATAL_ERROR "download failed: ${url}: ${msg}")
    endif()
endfunction()

# --- SDL3
set(sdl_ok OFF)
if(NOT FORCE AND EXISTS "${DEST}/SDL3/REVISION.txt")
    file(STRINGS "${DEST}/SDL3/REVISION.txt" rev REGEX "^release-${SDL_VERSION}-")
    if(rev AND EXISTS "${DEST}/SDL3/android-java/SDLActivity.java")
        set(sdl_ok ON)
    endif()
endif()
if(sdl_ok)
    message(STATUS "SDL3 ${SDL_VERSION}: up to date")
else()
    message(STATUS "SDL3 ${SDL_VERSION}:")
    fetch("${SDL_URL}" "${tmp}/sdl.tar.gz" ${SDL_SHA256})
    file(ARCHIVE_EXTRACT INPUT "${tmp}/sdl.tar.gz" DESTINATION "${tmp}")
    set(src "${tmp}/SDL3-${SDL_VERSION}")
    file(REMOVE_RECURSE "${DEST}/SDL3")
    file(MAKE_DIRECTORY "${DEST}/SDL3")
    foreach(item IN LISTS SDL_KEEP)
        file(COPY "${src}/${item}" DESTINATION "${DEST}/SDL3")
    endforeach()
    foreach(item IN LISTS SDL_DROP)
        file(REMOVE_RECURSE "${DEST}/SDL3/${item}")
    endforeach()
    file(GLOB java "${src}/${SDL_ANDROID_JAVA}/*.java")
    file(COPY ${java} DESTINATION "${DEST}/SDL3/android-java")
    message(STATUS "  -> ${DEST}/SDL3")
endif()

# --- stb
file(MAKE_DIRECTORY "${DEST}/stb")
foreach(name IN LISTS STB_FILES)
    set(sum ${STB_SHA256_${name}})
    set(have "")
    if(NOT FORCE AND EXISTS "${DEST}/stb/${name}")
        file(SHA256 "${DEST}/stb/${name}" have)
    endif()
    if(have STREQUAL sum)
        message(STATUS "stb/${name}: up to date")
    else()
        message(STATUS "stb/${name}:")
        fetch("https://raw.githubusercontent.com/nothings/stb/${STB_COMMIT}/${name}" "${tmp}/${name}" ${sum})
        file(COPY "${tmp}/${name}" DESTINATION "${DEST}/stb")
    endif()
endforeach()

file(REMOVE_RECURSE "${tmp}")
message(STATUS "done")
