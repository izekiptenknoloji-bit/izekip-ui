add_executable(IzekipUIExample02 main.cpp)
target_link_libraries(IzekipUIExample02 PRIVATE IzekipUI)

add_custom_command(TARGET IzekipUIExample02 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/style.css"
        "$<TARGET_FILE_DIR:IzekipUIExample02>/style.css"
)

add_custom_command(TARGET IzekipUIExample02 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/third_party/fonts/Inter.ttf"
        "$<TARGET_FILE_DIR:IzekipUIExample02>/Inter.ttf"
)

add_custom_command(TARGET IzekipUIExample02 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/third_party/fonts/SpaceMono-Regular.ttf"
        "$<TARGET_FILE_DIR:IzekipUIExample02>/SpaceMono-Regular.ttf"
)

if(IZEKIP_RENDER_BACKEND STREQUAL "D3D11")
    add_custom_command(TARGET IzekipUIExample02 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/src/Renderer/Shaders/RectShader.hlsl"
            "$<TARGET_FILE_DIR:IzekipUIExample02>/RectShader.hlsl"
    )
elseif(IZEKIP_RENDER_BACKEND STREQUAL "VULKAN")
    add_custom_command(TARGET IzekipUIExample02 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/generated/Shaders/RectShader.vert.spv"
            "$<TARGET_FILE_DIR:IzekipUIExample02>/RectShader.vert.spv"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/generated/Shaders/RectShader.frag.spv"
            "$<TARGET_FILE_DIR:IzekipUIExample02>/RectShader.frag.spv"
    )
endif()
