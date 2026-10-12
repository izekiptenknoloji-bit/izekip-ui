# Provides izekip_ui_copy_runtime_assets(TARGET <name>) -- the installed, reusable
# generalization of the asset-copy add_custom_command() blocks every example in this repo's
# own examples/*/CMakeLists.txt otherwise has to hand-roll individually (fonts always;
# shader source or precompiled .spv depending on which backend this particular install was
# built with). A consumer's own style.css is NOT copied here -- that stays each project's
# own responsibility, since its name/location isn't something this library can guess.

# Captured now, while CMAKE_CURRENT_LIST_DIR genuinely refers to this file's own directory
# (<prefix>/lib/cmake/IzekipUI). Do NOT use CMAKE_CURRENT_LIST_DIR *inside* the function
# below -- once a consumer calls it, CMAKE_CURRENT_LIST_DIR reflects the CALLER's listfile,
# not this one.
set(_izekip_ui_install_prefix "${CMAKE_CURRENT_LIST_DIR}/../../..")
get_filename_component(_izekip_ui_install_prefix "${_izekip_ui_install_prefix}" ABSOLUTE)

function(izekip_ui_copy_runtime_assets)
    cmake_parse_arguments(ARG "" "TARGET" "" ${ARGN})
    if(NOT ARG_TARGET)
        message(FATAL_ERROR "izekip_ui_copy_runtime_assets: TARGET is required")
    endif()

    set(_fonts_dir "${_izekip_ui_install_prefix}/share/IzekipUI/fonts")
    set(_shaders_dir "${_izekip_ui_install_prefix}/share/IzekipUI/shaders")

    add_custom_command(TARGET ${ARG_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_fonts_dir}/Inter.ttf"
            "$<TARGET_FILE_DIR:${ARG_TARGET}>/Inter.ttf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_fonts_dir}/SpaceMono-Regular.ttf"
            "$<TARGET_FILE_DIR:${ARG_TARGET}>/SpaceMono-Regular.ttf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_fonts_dir}/Arimo-Regular.ttf"
            "$<TARGET_FILE_DIR:${ARG_TARGET}>/Arimo-Regular.ttf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_fonts_dir}/Tinos-Regular.ttf"
            "$<TARGET_FILE_DIR:${ARG_TARGET}>/Tinos-Regular.ttf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_fonts_dir}/Cousine-Regular.ttf"
            "$<TARGET_FILE_DIR:${ARG_TARGET}>/Cousine-Regular.ttf"
    )

    if(IzekipUI_RENDER_BACKEND STREQUAL "VULKAN")
        add_custom_command(TARGET ${ARG_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_shaders_dir}/RectShader.vert.spv"
                "$<TARGET_FILE_DIR:${ARG_TARGET}>/RectShader.vert.spv"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_shaders_dir}/RectShader.frag.spv"
                "$<TARGET_FILE_DIR:${ARG_TARGET}>/RectShader.frag.spv"
        )
    else()
        add_custom_command(TARGET ${ARG_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_shaders_dir}/RectShader.hlsl"
                "$<TARGET_FILE_DIR:${ARG_TARGET}>/RectShader.hlsl"
        )
    endif()
endfunction()
