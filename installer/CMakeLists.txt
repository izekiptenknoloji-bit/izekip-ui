# IzekipUISetup -- the SDK installer, built WITH İzekip UI (dogfooding) but never part of
# the library itself: all Win32 code lives here (WinSys.cpp), not under src/. Windows +
# D3D11 only (D3D11 is always present on Windows; the installer must not need a Vulkan
# runtime just to install a Vulkan SDK payload). Not installed by `cmake --install` --
# cmake/PackageSDK.cmake picks it up from the build tree.
if(NOT WIN32 OR NOT IZEKIP_RENDER_BACKEND STREQUAL "D3D11")
    message(STATUS "IzekipUISetup: skipped (needs WIN32 + the D3D11 backend)")
    return()
endif()

configure_file(IzekipUISetup.rc.in "${CMAKE_CURRENT_BINARY_DIR}/IzekipUISetup.rc" @ONLY)

add_executable(IzekipUISetup WIN32
    main.cpp
    CommandLine.cpp
    Manifest.cpp
    InstallEngine.cpp
    WinSys.cpp
    InstallerApp.cpp
    IzekipUISetup.manifest
    "${CMAKE_CURRENT_BINARY_DIR}/IzekipUISetup.rc"
)
# WIN32 subsystem (no console window) but keep a plain int main().
target_link_options(IzekipUISetup PRIVATE /ENTRY:mainCRTStartup)
target_link_libraries(IzekipUISetup PRIVATE IzekipUI shell32 ole32 advapi32 uuid)
# The release exe keeps the same name in Debug builds -- the uninstaller copies itself by
# this exact file name (InstallEngine::SelfAssetNames).
set_target_properties(IzekipUISetup PROPERTIES DEBUG_POSTFIX "")

add_custom_command(TARGET IzekipUISetup POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/setup.css" "$<TARGET_FILE_DIR:IzekipUISetup>/setup.css"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/third_party/fonts/Inter.ttf" "$<TARGET_FILE_DIR:IzekipUISetup>/Inter.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/third_party/fonts/SpaceMono-Regular.ttf"
        "$<TARGET_FILE_DIR:IzekipUISetup>/SpaceMono-Regular.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/src/Renderer/Shaders/RectShader.hlsl" "$<TARGET_FILE_DIR:IzekipUISetup>/RectShader.hlsl"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/logo.svg" "$<TARGET_FILE_DIR:IzekipUISetup>/logo.svg"
)
