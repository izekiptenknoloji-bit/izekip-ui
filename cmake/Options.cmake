if(WIN32)
    set(IZEKIP_DEFAULT_RENDER_BACKEND "D3D11")
else()
    set(IZEKIP_DEFAULT_RENDER_BACKEND "VULKAN")
endif()
set(IZEKIP_RENDER_BACKEND "${IZEKIP_DEFAULT_RENDER_BACKEND}" CACHE STRING "Render backend (D3D11 or VULKAN)")
set_property(CACHE IZEKIP_RENDER_BACKEND PROPERTY STRINGS "D3D11" "VULKAN")

option(IZEKIP_BUILD_EXAMPLES "Build IzekipUI example apps" ${PROJECT_IS_TOP_LEVEL})
option(IZEKIP_BUILD_TESTS "Build IzekipUI tests" ${PROJECT_IS_TOP_LEVEL})
option(IZEKIP_BUILD_CLI "Build the cssfr project-scaffolding CLI" ${PROJECT_IS_TOP_LEVEL})
option(IZEKIP_BUILD_INSTALLER "Build the IzekipUISetup SDK installer (Windows + D3D11 backend only)" OFF)

if(IZEKIP_RENDER_BACKEND STREQUAL "D3D11")
    if(NOT WIN32)
        message(FATAL_ERROR "The D3D11 backend is Windows-only; use -DIZEKIP_RENDER_BACKEND=VULKAN")
    endif()
    set(IZEKIP_RENDER_BACKEND_D3D11 1)
    set(IZEKIP_RENDER_BACKEND_VULKAN 0)
elseif(IZEKIP_RENDER_BACKEND STREQUAL "VULKAN")
    set(IZEKIP_RENDER_BACKEND_D3D11 0)
    set(IZEKIP_RENDER_BACKEND_VULKAN 1)
    find_package(Vulkan REQUIRED COMPONENTS dxc)
else()
    message(FATAL_ERROR "IZEKIP_RENDER_BACKEND must be D3D11 or VULKAN, got: ${IZEKIP_RENDER_BACKEND}")
endif()

configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/RenderBackendConfig.h.in"
    "${CMAKE_BINARY_DIR}/generated/Renderer/RenderBackendConfig.h"
)
