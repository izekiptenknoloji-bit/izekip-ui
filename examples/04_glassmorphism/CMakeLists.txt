add_executable(IzekipUIExample04 main.cpp)
target_link_libraries(IzekipUIExample04 PRIVATE IzekipUI)

add_custom_command(TARGET IzekipUIExample04 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/style.css"
        "$<TARGET_FILE_DIR:IzekipUIExample04>/style.css"
)

add_custom_command(TARGET IzekipUIExample04 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/third_party/fonts/Inter.ttf"
        "$<TARGET_FILE_DIR:IzekipUIExample04>/Inter.ttf"
)

add_custom_command(TARGET IzekipUIExample04 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/third_party/fonts/SpaceMono-Regular.ttf"
        "$<TARGET_FILE_DIR:IzekipUIExample04>/SpaceMono-Regular.ttf"
)

if(IZEKIP_RENDER_BACKEND STREQUAL "D3D11")
    add_custom_command(TARGET IzekipUIExample04 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/src/Renderer/Shaders/RectShader.hlsl"
            "$<TARGET_FILE_DIR:IzekipUIExample04>/RectShader.hlsl"
    )
elseif(IZEKIP_RENDER_BACKEND STREQUAL "VULKAN")
    add_custom_command(TARGET IzekipUIExample04 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/generated/Shaders/RectShader.vert.spv"
            "$<TARGET_FILE_DIR:IzekipUIExample04>/RectShader.vert.spv"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/generated/Shaders/RectShader.frag.spv"
            "$<TARGET_FILE_DIR:IzekipUIExample04>/RectShader.frag.spv"
    )
endif()
