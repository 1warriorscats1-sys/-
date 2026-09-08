# TH095 (東方文花帖 ~ Shoot the Bullet) — platform layer build.
#
# Builds the pinned upstream reconstruction (N0zoM1z0/th095, runnable
# non-DIFFBUILD mode) against the port platform layer in port/. The game
# sources are not modified in the repository; scripts/patch_upstream.py
# disables their 32-bit compile-time layout asserts on the fetched working
# copy (see scripts/patch_upstream.py for the full rationale).
#
# Host build (Linux, x86-64 or aarch64):
#   cmake -S th095 -B th095/build-host -DTH095_UPSTREAM_SRC=<src>
# Switch build (devkitA64):
#   cmake -S th095 -B th095/build-switch -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake
#
# Modeled on the TH08 port (N0zoM1z0/th08, saekaze/th08-switch), MIT.

cmake_minimum_required(VERSION 3.16)
project(th095-port CXX C)

set(TH095_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(TH095_UPSTREAM_SRC "" CACHE PATH "Path to the fetched TH095 reconstruction source")
if(NOT TH095_UPSTREAM_SRC)
    set(TH095_UPSTREAM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../build-inputs/th095-src)
endif()
if(NOT IS_ABSOLUTE ${TH095_UPSTREAM_SRC})
    set(TH095_UPSTREAM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/${TH095_UPSTREAM_SRC})
endif()
if(NOT EXISTS ${TH095_UPSTREAM_SRC}/src/Main.cpp)
    message(FATAL_ERROR
        "TH095 upstream source not found at ${TH095_UPSTREAM_SRC}.\n"
        "Run: python3 th095/scripts/fetch_upstream.py\n"
        "     python3 th095/scripts/patch_upstream.py build-inputs/th095-src")
endif()

file(GLOB TH095_GAME_SOURCES
    ${TH095_UPSTREAM_SRC}/src/*.cpp
    ${TH095_UPSTREAM_SRC}/src/ecl/*.cpp
    ${TH095_UPSTREAM_SRC}/src/pbg/*.cpp)

set(TH095_COMPAT_SOURCES
    ${TH095_PORT_DIR}/port/compat/win32_compat.cpp
    ${TH095_PORT_DIR}/port/compat/d3d8_compat.cpp
    ${TH095_PORT_DIR}/port/compat/d3dx8_compat.cpp
    # Upstream declares but does not define ~230 production symbols (the
    # VC7.1 link is documented as "failing closed"); these TUs close the gap.
    ${TH095_PORT_DIR}/port/compat/th095_missing.cpp
    ${TH095_PORT_DIR}/port/compat/th095_missing_globals.cpp)

if(NINTENDO_SWITCH)
    list(APPEND TH095_COMPAT_SOURCES ${TH095_PORT_DIR}/port/runtime/switch_runtime.cpp)
else()
    list(APPEND TH095_COMPAT_SOURCES ${TH095_PORT_DIR}/port/runtime/linux_runtime.cpp)
endif()

add_executable(th095-port ${TH095_GAME_SOURCES} ${TH095_COMPAT_SOURCES})

target_compile_definitions(th095-port PRIVATE TH095_MODERN_PORT=1)

# The reconstruction is MSVC-2003 i386 code; the port compiles it with the
# host/switch compiler, so loosen the same way the TH08 port does.
target_compile_options(th095-port PRIVATE
    -O2
    -fno-strict-aliasing
    -fpermissive
    -fno-omit-frame-pointer
    -Wno-write-strings
    "-include${TH095_PORT_DIR}/port/compat/th095_compat.hpp")

# Order matters: the fake Win32/DX headers come first so the untouched game
# sources pick them up, then the compat internals, then the upstream sources.
target_include_directories(th095-port BEFORE PRIVATE
    ${TH095_PORT_DIR}/port/include
    ${TH095_PORT_DIR}/port/compat
    ${TH095_UPSTREAM_SRC}/src)

if(NINTENDO_SWITCH)
    # devkitA64 (Switch.cmake) already sets -march, fPIE, __SWITCH__, -lnx.
    # portlibs (SDL2, SDL2_ttf, SDL2_image, EGL/GLESv2, ...) are static.
    find_package(SDL2 REQUIRED)
    target_link_libraries(th095-port PRIVATE
        SDL2::SDL2
        SDL2::SDL2_ttf
        SDL2::SDL2_image
        EGL
        GLESv2)
    set_target_properties(th095-port PROPERTIES OUTPUT_NAME "th095")

    if(EXISTS ${CMAKE_SOURCE_DIR}/icon.jpg)
        nx_generate_nacp(OUTPUT "${CMAKE_BINARY_DIR}/th095-base.nacp"
            NAME "Shoot the Bullet"
            AUTHOR "ZUN / Team Shanghai Alice")
        nx_link_options(-nro -g
            -nexflags "--unique=th095"
            "--sdk-version=19.0.1"
            "--titleid=010000000005A1E1"
            "--nro-header=${CMAKE_BINARY_DIR}/th095.nhdr")
        set_target_properties(th095-port PROPERTIES
            LINK_DEPENDS "${CMAKE_BINARY_DIR}/th095.nhdr")
    endif()
else()
    find_package(OpenGL REQUIRED)
    # pkg-config: distro SDL2(-ttf/-image) packages do not ship CMake config
    # files, but they all ship .pc files.
    # Note: module-name lookup is case-sensitive in the pkgconf shipped on
    # the ubuntu-latest (noble) runner — "SDL2_ttf"/"SDL2_image" resolve
    # against their exact-case .pc files while "SDL2" does NOT resolve
    # against the shipped "sdl2.pc". Try the canonical case first, then the
    # exact lowercase filename case (other distros keep working either way).
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2_REQ QUIET SDL2 SDL2_ttf SDL2_image)
    if(NOT SDL2_REQ_FOUND)
        pkg_check_modules(SDL2_REQ REQUIRED sdl2 SDL2_ttf SDL2_image)
    endif()
    find_library(FONTCONFIG_LIBRARY fontconfig)
    find_library(PTHREAD_LIBRARY pthread)

    target_link_libraries(th095-port PRIVATE
        ${SDL2_REQ_LIBRARIES}
        ${OPENGL_LIBRARIES}
        ${FONTCONFIG_LIBRARY}
        ${PTHREAD_LIBRARY}
        m
        dl)
    target_include_directories(th095-port PRIVATE ${SDL2_REQ_INCLUDE_DIRS})
    set_target_properties(th095-port PROPERTIES OUTPUT_NAME "th095-host")
endif()
