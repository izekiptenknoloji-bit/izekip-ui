add_executable(IzekipUICssParserTests CssParserTests.cpp)
target_link_libraries(IzekipUICssParserTests PRIVATE IzekipUI)

add_test(NAME CssParserTests COMMAND IzekipUICssParserTests)

add_executable(IzekipUITextBufferTests TextBufferTests.cpp)
target_link_libraries(IzekipUITextBufferTests PRIVATE IzekipUI)

add_test(NAME TextBufferTests COMMAND IzekipUITextBufferTests)

add_executable(IzekipUIInteractionTests InteractionTests.cpp)
target_link_libraries(IzekipUIInteractionTests PRIVATE IzekipUI)
add_test(NAME InteractionTests COMMAND IzekipUIInteractionTests)

# GPU-dependent: opens a window and needs a working D3D11/Vulkan driver. Off by default so the
# default test run stays headless (CI, build servers).
option(IZEKIP_TEST_GPU "Run GPU/display-dependent tests (needs a window and a GPU driver)" OFF)
if(IZEKIP_TEST_GPU)
    add_executable(IzekipUIRendererRecoveryTest RendererRecoveryTest.cpp)
    target_link_libraries(IzekipUIRendererRecoveryTest PRIVATE IzekipUI)
    add_test(NAME RendererRecoveryTest
        COMMAND IzekipUIRendererRecoveryTest
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:IzekipUIRendererRecoveryTest>")

    set(IZEKIP_RECOVERY_ASSETS
        "${CMAKE_SOURCE_DIR}/third_party/fonts/Inter.ttf"
        "${CMAKE_SOURCE_DIR}/third_party/fonts/SpaceMono-Regular.ttf")
    if(IZEKIP_RENDER_BACKEND STREQUAL "D3D11")
        list(APPEND IZEKIP_RECOVERY_ASSETS "${CMAKE_SOURCE_DIR}/src/Renderer/Shaders/RectShader.hlsl")
    else()
        list(APPEND IZEKIP_RECOVERY_ASSETS
            "${CMAKE_BINARY_DIR}/generated/Shaders/RectShader.vert.spv"
            "${CMAKE_BINARY_DIR}/generated/Shaders/RectShader.frag.spv")
    endif()
    add_custom_command(TARGET IzekipUIRendererRecoveryTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${IZEKIP_RECOVERY_ASSETS}
            "$<TARGET_FILE_DIR:IzekipUIRendererRecoveryTest>")
endif()

# Headless, files-only (--no-system) install -> uninstall round trip of the real installer
# exe against a fake payload. Registry/PATH/env/shortcut/elevation are verified by hand.
if(TARGET IzekipUISetup)
    add_test(NAME InstallerRoundTrip
        COMMAND ${CMAKE_COMMAND}
            "-DSETUP_EXE=$<TARGET_FILE:IzekipUISetup>"
            "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/installer_roundtrip"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/InstallerRoundTrip.cmake")
endif()
