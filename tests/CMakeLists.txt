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
    set(IZEKIP_GPU_TEST_ASSETS
        "${CMAKE_SOURCE_DIR}/third_party/fonts/Inter.ttf"
        "${CMAKE_SOURCE_DIR}/third_party/fonts/SpaceMono-Regular.ttf"
        "${CMAKE_SOURCE_DIR}/third_party/fonts/Arimo-Regular.ttf"
        "${CMAKE_SOURCE_DIR}/third_party/fonts/Tinos-Regular.ttf"
        "${CMAKE_SOURCE_DIR}/third_party/fonts/Cousine-Regular.ttf")
    if(IZEKIP_RENDER_BACKEND STREQUAL "D3D11")
        list(APPEND IZEKIP_GPU_TEST_ASSETS "${CMAKE_SOURCE_DIR}/src/Renderer/Shaders/RectShader.hlsl")
    else()
        list(APPEND IZEKIP_GPU_TEST_ASSETS
            "${CMAKE_BINARY_DIR}/generated/Shaders/RectShader.vert.spv"
            "${CMAKE_BINARY_DIR}/generated/Shaders/RectShader.frag.spv")
    endif()

    # Each GPU test runs from its own output folder, where the runtime assets are copied.
    function(izekip_add_gpu_test NAME SOURCE)
        add_executable(${NAME} ${SOURCE})
        target_link_libraries(${NAME} PRIVATE IzekipUI)
        add_test(NAME ${NAME} COMMAND ${NAME} WORKING_DIRECTORY "$<TARGET_FILE_DIR:${NAME}>")
        add_custom_command(TARGET ${NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${IZEKIP_GPU_TEST_ASSETS}
                "$<TARGET_FILE_DIR:${NAME}>")
    endfunction()

    izekip_add_gpu_test(IzekipUIRendererRecoveryTest RendererRecoveryTest.cpp)
    izekip_add_gpu_test(IzekipUIFontFamilyTest FontFamilyTest.cpp)
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

add_executable(IzekipUISafeModeTests SafeModeTests.cpp)
target_link_libraries(IzekipUISafeModeTests PRIVATE IzekipUI)
add_test(NAME SafeModeTests COMMAND IzekipUISafeModeTests)

add_executable(IzekipUIGlyphAtlasTests GlyphAtlasTests.cpp)
target_link_libraries(IzekipUIGlyphAtlasTests PRIVATE IzekipUI)
target_compile_definitions(IzekipUIGlyphAtlasTests PRIVATE
    IZEKIP_TEST_FONT_DIR="${CMAKE_SOURCE_DIR}/third_party/fonts")
add_test(NAME GlyphAtlasTests COMMAND IzekipUIGlyphAtlasTests)

add_executable(IzekipUITextAreaTests TextAreaTests.cpp)
target_link_libraries(IzekipUITextAreaTests PRIVATE IzekipUI)
# TextArea resolves its font by file name next to the executable, so the test copies Inter there.
add_custom_command(TARGET IzekipUITextAreaTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/third_party/fonts/Inter.ttf" "$<TARGET_FILE_DIR:IzekipUITextAreaTests>/Inter.ttf")
add_test(NAME TextAreaTests COMMAND IzekipUITextAreaTests)

add_executable(IzekipUIDiagnosticsTests DiagnosticsTests.cpp)
target_link_libraries(IzekipUIDiagnosticsTests PRIVATE IzekipUI)
add_test(NAME DiagnosticsTests COMMAND IzekipUIDiagnosticsTests)

add_executable(IzekipUIFrameTests FrameTests.cpp)
target_link_libraries(IzekipUIFrameTests PRIVATE IzekipUI)
add_test(NAME FrameTests COMMAND IzekipUIFrameTests)
