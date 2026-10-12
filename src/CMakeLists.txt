set(IZEKIP_SOURCES
    Core/Color.cpp
    Core/StyleProperties.cpp
    Core/Widget.cpp
    Core/Easing.cpp
    Core/Interpolate.cpp
    Core/TextBuffer.cpp
    Core/Utf8.cpp
    Core/AssetPath.cpp
    Platform/SafeMode.cpp
    Core/InteractionController.cpp
    Widgets/Panel.cpp
    Widgets/Button.cpp
    Widgets/Label.cpp
    Widgets/TextArea.cpp
    Widgets/Checkbox.cpp
    Widgets/RadioButton.cpp
    Widgets/ProgressBar.cpp
    Widgets/Image.cpp
    Platform/Window.cpp
    Renderer/RHI.cpp
    CSS/CssTokenizer.cpp
    CSS/StyleSheet.cpp
    CSS/StyleApplier.cpp
    CSS/LexborBridge.cpp
    Renderer/RectDrawData.cpp
    Renderer/GlyphAtlas.cpp
)

if(IZEKIP_RENDER_BACKEND STREQUAL "D3D11")
    list(APPEND IZEKIP_SOURCES Renderer/D3D11/D3D11Renderer.cpp)
elseif(IZEKIP_RENDER_BACKEND STREQUAL "VULKAN")
    list(APPEND IZEKIP_SOURCES Renderer/Vulkan/VulkanRenderer.cpp)
endif()

add_library(IzekipUI STATIC ${IZEKIP_SOURCES})

# include(GNUInstallDirs) already called once, at the top-level CMakeLists.txt.

target_include_directories(IzekipUI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/third_party/stb
)

target_link_libraries(IzekipUI
    PUBLIC
        glfw
        yogacore
        lexbor_static
)

if(IZEKIP_RENDER_BACKEND STREQUAL "D3D11")
    target_link_libraries(IzekipUI PUBLIC d3d11 dxgi d3dcompiler)
elseif(IZEKIP_RENDER_BACKEND STREQUAL "VULKAN")
    target_link_libraries(IzekipUI PUBLIC Vulkan::Vulkan)

    set(IZEKIP_SHADER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Renderer/Shaders/RectShader.hlsl")
    set(IZEKIP_SHADER_OUT_DIR "${CMAKE_BINARY_DIR}/generated/Shaders")
    file(MAKE_DIRECTORY ${IZEKIP_SHADER_OUT_DIR})

    add_custom_command(
        OUTPUT "${IZEKIP_SHADER_OUT_DIR}/RectShader.vert.spv"
        COMMAND $<TARGET_FILE:Vulkan::dxc_exe> -spirv -T vs_6_0 -E VSMain
                -D VK_BACKEND=1 -Fo "${IZEKIP_SHADER_OUT_DIR}/RectShader.vert.spv"
                "${IZEKIP_SHADER_SRC}"
        DEPENDS "${IZEKIP_SHADER_SRC}"
        COMMENT "Compiling RectShader.hlsl (VSMain) to SPIR-V"
    )
    add_custom_command(
        OUTPUT "${IZEKIP_SHADER_OUT_DIR}/RectShader.frag.spv"
        COMMAND $<TARGET_FILE:Vulkan::dxc_exe> -spirv -T ps_6_0 -E PSMain
                -D VK_BACKEND=1 -Fo "${IZEKIP_SHADER_OUT_DIR}/RectShader.frag.spv"
                "${IZEKIP_SHADER_SRC}"
        DEPENDS "${IZEKIP_SHADER_SRC}"
        COMMENT "Compiling RectShader.hlsl (PSMain) to SPIR-V"
    )
    add_custom_target(IzekipUIShaders DEPENDS
        "${IZEKIP_SHADER_OUT_DIR}/RectShader.vert.spv"
        "${IZEKIP_SHADER_OUT_DIR}/RectShader.frag.spv")
    add_dependencies(IzekipUI IzekipUIShaders)
endif()
