add_executable(IzekipUIExample03 main.cpp)
target_link_libraries(IzekipUIExample03 PRIVATE IzekipUI)

add_custom_command(TARGET IzekipUIExample03 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/style.css"
        "$<TARGET_FILE_DIR:IzekipUIExample03>/style.css"
)

add_custom_command(TARGET IzekipUIExample03 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/third_party/fonts/Inter.ttf"
        "$<TARGET_FILE_DIR:IzekipUIExample03>/Inter.ttf"
)

add_custom_command(TARGET IzekipUIExample03 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/third_party/fonts/SpaceMono-Regular.ttf"
        "$<TARGET_FILE_DIR:IzekipUIExample03>/SpaceMono-Regular.ttf"
)

if(IZEKIP_RENDER_BACKEND STREQUAL "D3D11")
    add_custom_command(TARGET IzekipUIExample03 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/src/Renderer/Shaders/RectShader.hlsl"
            "$<TARGET_FILE_DIR:IzekipUIExample03>/RectShader.hlsl"
    )
elseif(IZEKIP_RENDER_BACKEND STREQUAL "VULKAN")
    add_custom_command(TARGET IzekipUIExample03 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/generated/Shaders/RectShader.vert.spv"
            "$<TARGET_FILE_DIR:IzekipUIExample03>/RectShader.vert.spv"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/generated/Shaders/RectShader.frag.spv"
            "$<TARGET_FILE_DIR:IzekipUIExample03>/RectShader.frag.spv"
    )
endif()
