# Builds the distributable SDK zip: installer exe + its assets + payload/<backend>/ (a
# Debug+Release `cmake --install` per backend). CMake script, no Python.
#
#   cmake -DD3D11_BUILD_DIR=build [-DVULKAN_BUILD_DIR=build-vulkan] [-DOUT_DIR=dist] -P cmake/PackageSDK.cmake
#
# Signing is NOT part of this script (see COMPATIBILITY.md). To sign, run signtool on
# <stage>/IzekipUISetup.exe and <stage>/payload/*/bin/cssfr.exe, then re-zip.
cmake_minimum_required(VERSION 3.21)

get_filename_component(SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT D3D11_BUILD_DIR)
    message(FATAL_ERROR "D3D11_BUILD_DIR is required (the installer is built from the D3D11 build)")
endif()
get_filename_component(D3D11_BUILD_DIR "${D3D11_BUILD_DIR}" ABSOLUTE)
if(VULKAN_BUILD_DIR)
    get_filename_component(VULKAN_BUILD_DIR "${VULKAN_BUILD_DIR}" ABSOLUTE)
endif()
if(NOT OUT_DIR)
    set(OUT_DIR "${SOURCE_DIR}/dist")
endif()
get_filename_component(OUT_DIR "${OUT_DIR}" ABSOLUTE)

function(run)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "command failed (${result}): ${ARGN}")
    endif()
endfunction()

function(read_version build_dir out_var)
    file(STRINGS "${build_dir}/generated/Core/Version.h" line REGEX "IZEKIP_VERSION_STRING")
    string(REGEX MATCH "\"([^\"]+)\"" _ "${line}")
    set(${out_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

# 1. (Re)configure: the D3D11 build must include the installer target.
run(${CMAKE_COMMAND} -S "${SOURCE_DIR}" -B "${D3D11_BUILD_DIR}" -DIZEKIP_BUILD_INSTALLER=ON)
read_version("${D3D11_BUILD_DIR}" VERSION)
if(VULKAN_BUILD_DIR)
    run(${CMAKE_COMMAND} -S "${SOURCE_DIR}" -B "${VULKAN_BUILD_DIR}")
    read_version("${VULKAN_BUILD_DIR}" VULKAN_VERSION)
    if(NOT VULKAN_VERSION STREQUAL VERSION)
        message(FATAL_ERROR "version mismatch: D3D11 build ${VERSION}, Vulkan build ${VULKAN_VERSION}")
    endif()
endif()
message(STATUS "Packaging IzekipUI SDK ${VERSION}")

set(STAGE_NAME "IzekipUI-SDK-${VERSION}")
set(STAGE "${OUT_DIR}/${STAGE_NAME}")
file(REMOVE_RECURSE "${STAGE}")
file(REMOVE "${OUT_DIR}/${STAGE_NAME}-win64.zip")

# 2. Build + install Debug THEN Release into each payload: libraries coexist (the Debug ones
#    carry a `d` suffix), and Release goes last so the shared files that differ -- notably
#    bin/cssfr.exe -- end up as Release builds (a Debug exe needs debug CRT DLLs end users
#    don't have).
function(build_and_install build_dir payload_name)
    foreach(config Debug Release)
        run(${CMAKE_COMMAND} --build "${build_dir}" --config ${config})
        run(${CMAKE_COMMAND} --install "${build_dir}" --config ${config} --prefix "${STAGE}/payload/${payload_name}")
    endforeach()
endfunction()

build_and_install("${D3D11_BUILD_DIR}" d3d11)
set(BACKENDS "d3d11")
if(VULKAN_BUILD_DIR)
    build_and_install("${VULKAN_BUILD_DIR}" vulkan)
    set(BACKENDS "d3d11 vulkan")
endif()

# 3. Installer exe + the runtime assets it loads from its own folder.
set(SETUP_DIR "${D3D11_BUILD_DIR}/installer/Release")
foreach(f IzekipUISetup.exe setup.css Inter.ttf SpaceMono-Regular.ttf RectShader.hlsl)
    if(NOT EXISTS "${SETUP_DIR}/${f}")
        message(FATAL_ERROR "missing installer file: ${SETUP_DIR}/${f}")
    endif()
    file(COPY "${SETUP_DIR}/${f}" DESTINATION "${STAGE}")
endforeach()

file(WRITE "${STAGE}/payload/payload.info" "version=${VERSION}\nbackends=${BACKENDS}\n")
file(WRITE "${STAGE}/BENIOKU.txt"
"İzekip UI SDK ${VERSION}

Kurulum:
  1. Bu zip arşivini bir klasöre çıkarın (zip içinden doğrudan çalıştırmayın).
  2. IzekipUISetup.exe dosyasını çalıştırın ve adımları izleyin.

Gereksinimler:
  - Windows 10 veya üstü, 64-bit.
  - Microsoft Visual C++ 2015-2022 Redistributable (x64) -- Visual Studio kuruluysa zaten vardır.
  - SDK'yı kullanmak için: Visual Studio (MSVC) ve CMake 3.21+.
  - Vulkan payload'ını seçerseniz: Vulkan SDK.

Kaldırma: Ayarlar > Uygulamalar > \"İzekip UI SDK\" > Kaldır
(ya da <kurulum klasörü>\\uninstall\\IzekipUISetup.exe --uninstall).
")

# 4. Zip with a single top-level folder.
run(${CMAKE_COMMAND} -E chdir "${OUT_DIR}" ${CMAKE_COMMAND} -E tar cf "${STAGE_NAME}-win64.zip" --format=zip "${STAGE_NAME}")
message(STATUS "Wrote ${OUT_DIR}/${STAGE_NAME}-win64.zip")
