cmake_minimum_required(VERSION 3.21)
project(IzekipUI VERSION 1.3.0 LANGUAGES CXX)

# Public (consumer-visible) version header -- see COMPATIBILITY.md for what "1.0.0" means
# for this library (source-level compatibility, Tier-1 headers only) and the release
# checklist that keeps this in sync with CHANGELOG.md/git tags.
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/Version.h.in"
    "${CMAKE_BINARY_DIR}/generated/Core/Version.h"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/Options.cmake)

# Needed before the yogacore include-dir fix right below (CMAKE_INSTALL_INCLUDEDIR), and
# before cmake/Install.cmake later -- included once, here, rather than twice.
include(GNUInstallDirs)

# Debug libraries get a `d` suffix (IzekipUId.lib, glfw3d.lib, ...) so Debug and Release can
# both be installed into the same prefix -- a consumer building Debug against a Release-only
# install would otherwise fail to link (LNK2038: /MD vs /MDd, iterator debug level). Set
# before the third_party subdirectories so their library targets pick it up too; it only
# applies to libraries, executables keep their names.
set(CMAKE_DEBUG_POSTFIX d)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/glfw)

add_subdirectory(third_party/yoga/yoga)

# yogacore's own target_include_directories() (third_party/yoga/yoga/CMakeLists.txt) bakes
# in $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include/yoga> -- one directory level off
# from where our own `#include <yoga/Yoga.h>` expects it, AND (discovered by actually
# running the end-to-end SDK verification, not just reasoning about it) that path is fixed
# at *this* configure's CMAKE_INSTALL_PREFIX regardless of what --prefix a later
# `cmake --install` call uses, so a consumer project's find_package(IzekipUI) hit a hard
# "includes non-existent path" error, not a harmless unused flag as first assumed. Overwrite
# it outright with the same correct, relocatable interface IzekipUI's own target uses --
# we never patch the vendored submodule file itself, just this target's CMake-level
# property after the fact.
set_target_properties(yogacore PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third_party/yoga>;$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set(LEXBOR_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_WITH_FUZZER OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_UTILS OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/lexbor)

if(MSVC)
    add_compile_options(/utf-8)
endif()

add_subdirectory(src)

if(IZEKIP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(IZEKIP_BUILD_CLI)
    add_subdirectory(cli)
endif()

# Before tests/: the installer round-trip test is only registered if this target exists.
if(IZEKIP_BUILD_INSTALLER)
    add_subdirectory(installer)
endif()

if(IZEKIP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# install()/find_package() support -- see COMPATIBILITY.md. Only when IzekipUI itself is
# the top-level project: a consumer still add_subdirectory()-ing IzekipUI per the existing
# source-consumption model shouldn't get surprise install() rules of its own. Deliberately
# LAST: it checks if(TARGET cssfr), which only exists once add_subdirectory(cli) above has
# already run (found by actually running the install and noticing cssfr.exe never showed up
# -- an earlier version of this file had this include too early).
if(PROJECT_IS_TOP_LEVEL)
    include(cmake/Install.cmake)
    include(cmake/Packaging.cmake) # Linux .deb via CPack (no-op elsewhere)
endif()
