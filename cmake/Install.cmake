# install()/find_package() support for IzekipUI. Only processed when IzekipUI is the
# top-level project (see the if(PROJECT_IS_TOP_LEVEL) guard in the top-level CMakeLists.txt)
# -- a project that still add_subdirectory()s IzekipUI per COMPATIBILITY.md's existing
# source-consumption model shouldn't get surprise install() rules of its own.
#
# One export set ("IzekipUI-export") bundles IzekipUI itself plus every vendored dependency
# (glfw, yogacore, lexbor_static) so a consumer's single find_package(IzekipUI) resolves
# everything via the one IzekipUI::IzekipUI target -- the three dependencies are never named
# directly by consumer code, they're already PUBLIC-linked into IzekipUI (see
# src/CMakeLists.txt).

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS IzekipUI glfw yogacore lexbor_static
    EXPORT IzekipUI-export
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# cssfr -- the scaffolding CLI, a plain self-contained executable (not part of the
# find_package() export set, it has no library interface of its own to export; just a
# regular installed binary a consumer runs directly, e.g. <prefix>/bin/cssfr.exe).
if(TARGET cssfr)
    install(TARGETS cssfr RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# IzekipUI's own public + internal (Tier-2) headers -- COMPATIBILITY.md is explicit that
# Tier-2 headers (CssTokenizer.h, Interpolate.h, RectDrawData.h, GlyphAtlas.h) are "internal
# to specific .cpp files," not "forbidden to physically ship": several Tier-1 headers'
# own .cpp counterparts need them to compile against an installed copy.
install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# The two generated headers (Version.h, RenderBackendConfig.h) -- built-tree-only today,
# so they aren't covered by the install(DIRECTORY include/ ...) rule above.
install(FILES "${CMAKE_BINARY_DIR}/generated/Core/Version.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Core
)
install(FILES "${CMAKE_BINARY_DIR}/generated/Renderer/RenderBackendConfig.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Renderer
)

# GLFW's own install() is force-disabled (GLFW_INSTALL OFF, see top-level CMakeLists.txt)
# so its public header is otherwise never installed by anyone -- its target_include_directories
# is already correctly relocatable ($<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>), it
# just needs the header physically present.
install(DIRECTORY "${CMAKE_SOURCE_DIR}/third_party/glfw/include/GLFW/"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/GLFW
    FILES_MATCHING PATTERN "*.h"
)

# Yoga: the inner third_party/yoga/yoga/CMakeLists.txt actually built by this project has
# ZERO install() calls of its own (the real install/export machinery lives in the OUTER,
# never-processed third_party/yoga/CMakeLists.txt). Its target_include_directories() bakes
# a hardcoded, wrong-by-one-level $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include/yoga>
# -- the TOP-LEVEL CMakeLists.txt already overwrites yogacore's INTERFACE_INCLUDE_DIRECTORIES
# outright (right after its add_subdirectory() call) with the same correct, relocatable
# interface IzekipUI's own target uses, so by the time install(TARGETS ... yogacore ...)
# runs above, this is already fixed. (First attempt assumed the original wrong entry would
# just be a harmless unused extra -I flag -- it wasn't: running the actual end-to-end SDK
# verification against a real consumer project surfaced a hard "includes non-existent path"
# configure error, which is what prompted the real fix at the top level instead.) This rule
# installs yoga's headers where that corrected interface expects them: <prefix>/include/yoga/*.h,
# resolved via `#include <yoga/Yoga.h>` against <prefix>/include.
install(DIRECTORY "${CMAKE_SOURCE_DIR}/third_party/yoga/yoga/"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/yoga
    FILES_MATCHING PATTERN "*.h"
)

# lexbor installs its own headers/targets/package-config itself (LEXBOR_INSTALL_HEADERS
# defaults ON, never touched by this project) -- no action needed here. Its own
# lexbor-config.cmake/.pc files landing in the same prefix is a harmless, accepted side
# effect of not patching the vendored submodule's build logic.

# Runtime assets (fonts always; shader source or precompiled SPIR-V depending on which
# backend this install was built with) -- read back by cmake/IzekipUIAssets.cmake's
# izekip_ui_copy_runtime_assets() function at a consumer's build time.
install(FILES
    "${CMAKE_SOURCE_DIR}/third_party/fonts/Inter.ttf"
    "${CMAKE_SOURCE_DIR}/third_party/fonts/SpaceMono-Regular.ttf"
    DESTINATION share/IzekipUI/fonts
)

if(IZEKIP_RENDER_BACKEND STREQUAL "VULKAN")
    install(FILES
        "${CMAKE_BINARY_DIR}/generated/Shaders/RectShader.vert.spv"
        "${CMAKE_BINARY_DIR}/generated/Shaders/RectShader.frag.spv"
        DESTINATION share/IzekipUI/shaders
    )
else()
    install(FILES
        "${CMAKE_SOURCE_DIR}/src/Renderer/Shaders/RectShader.hlsl"
        DESTINATION share/IzekipUI/shaders
    )
endif()

# Docs -- also what the installer's Start Menu shortcut leads to.
install(FILES
    "${CMAKE_SOURCE_DIR}/README.md"
    "${CMAKE_SOURCE_DIR}/LICENSE"
    "${CMAKE_SOURCE_DIR}/CHANGELOG.md"
    "${CMAKE_SOURCE_DIR}/COMPATIBILITY.md"
    DESTINATION share/doc/IzekipUI
)

install(FILES
    "${CMAKE_SOURCE_DIR}/docs/ai-guide.md"
    RENAME AI_GUIDE.md
    DESTINATION share/doc/IzekipUI
)

install(EXPORT IzekipUI-export
    FILE IzekipUITargets.cmake
    NAMESPACE IzekipUI::
    DESTINATION lib/cmake/IzekipUI
)

configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmake/IzekipUIConfig.cmake.in"
    "${CMAKE_BINARY_DIR}/IzekipUIConfig.cmake"
    INSTALL_DESTINATION lib/cmake/IzekipUI
)
write_basic_package_version_file(
    "${CMAKE_BINARY_DIR}/IzekipUIConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_BINARY_DIR}/IzekipUIConfig.cmake"
    "${CMAKE_BINARY_DIR}/IzekipUIConfigVersion.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/IzekipUIAssets.cmake"
    DESTINATION lib/cmake/IzekipUI
)
