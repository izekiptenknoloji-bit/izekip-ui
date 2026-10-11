add_executable(IzekipUICssParserTests CssParserTests.cpp)
target_link_libraries(IzekipUICssParserTests PRIVATE IzekipUI)

add_test(NAME CssParserTests COMMAND IzekipUICssParserTests)

add_executable(IzekipUITextBufferTests TextBufferTests.cpp)
target_link_libraries(IzekipUITextBufferTests PRIVATE IzekipUI)

add_test(NAME TextBufferTests COMMAND IzekipUITextBufferTests)

add_executable(IzekipUIInteractionTests InteractionTests.cpp)
target_link_libraries(IzekipUIInteractionTests PRIVATE IzekipUI)
add_test(NAME InteractionTests COMMAND IzekipUIInteractionTests)

# Headless, files-only (--no-system) install -> uninstall round trip of the real installer
# exe against a fake payload. Registry/PATH/env/shortcut/elevation are verified by hand.
if(TARGET IzekipUISetup)
    add_test(NAME InstallerRoundTrip
        COMMAND ${CMAKE_COMMAND}
            "-DSETUP_EXE=$<TARGET_FILE:IzekipUISetup>"
            "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/installer_roundtrip"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/InstallerRoundTrip.cmake")
endif()
