# Font-family CSS names (arial, serif, courier) load these stand-ins from the executable's folder;
# without them the examples silently fall back to Inter. Each example calls this for its target.
function(izekip_example_fonts TARGET)
    foreach(font IN ITEMS Arimo-Regular.ttf Tinos-Regular.ttf Cousine-Regular.ttf)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/third_party/fonts/${font}"
                "$<TARGET_FILE_DIR:${TARGET}>/${font}")
    endforeach()
endfunction()

add_subdirectory(01_basic_panel_button)
add_subdirectory(02_text_document)
add_subdirectory(03_notepad)
add_subdirectory(04_glassmorphism)
