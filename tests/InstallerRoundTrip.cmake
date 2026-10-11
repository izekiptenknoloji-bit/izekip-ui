# Run as: cmake -DSETUP_EXE=<IzekipUISetup.exe> -DWORK_DIR=<scratch dir> -P InstallerRoundTrip.cmake
if(NOT SETUP_EXE OR NOT WORK_DIR)
    message(FATAL_ERROR "SETUP_EXE and WORK_DIR are required")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
set(PAYLOAD "${WORK_DIR}/payload")
file(WRITE "${PAYLOAD}/d3d11/bin/tool.txt" "tool")
file(WRITE "${PAYLOAD}/d3d11/include/Core/A.h" "// a")
file(WRITE "${PAYLOAD}/d3d11/share/doc/Türkçe ğüşı.txt" "türkçe")

set(PREFIX "${WORK_DIR}/İzekip Test")
file(MAKE_DIRECTORY "${PREFIX}")
file(WRITE "${PREFIX}/foreign.txt" "not ours -- must survive uninstall")

execute_process(COMMAND "${SETUP_EXE}" --headless --no-system "--payload=${PAYLOAD}" "--prefix=${PREFIX}"
                RESULT_VARIABLE result ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "install failed (${result}): ${err}")
endif()

foreach(f bin/tool.txt include/Core/A.h "share/doc/Türkçe ğüşı.txt"
          uninstall/install.manifest uninstall/IzekipUISetup.exe uninstall/setup.css uninstall/Inter.ttf)
    if(NOT EXISTS "${PREFIX}/${f}")
        message(FATAL_ERROR "missing after install: ${f}")
    endif()
endforeach()

file(READ "${PREFIX}/uninstall/install.manifest" manifest)
if(NOT manifest MATCHES "file=share/doc/Türkçe ğüşı.txt")
    message(FATAL_ERROR "manifest does not list the Turkish-named file:\n${manifest}")
endif()

# Reinstall over the same prefix (upgrade path) must also succeed.
execute_process(COMMAND "${SETUP_EXE}" --headless --no-system "--payload=${PAYLOAD}" "--prefix=${PREFIX}"
                RESULT_VARIABLE result ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "reinstall failed (${result}): ${err}")
endif()

execute_process(COMMAND "${SETUP_EXE}" --headless --no-system --uninstall --from-temp "--prefix=${PREFIX}"
                RESULT_VARIABLE result ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "uninstall failed (${result}): ${err}")
endif()

foreach(d bin include share uninstall)
    if(EXISTS "${PREFIX}/${d}")
        message(FATAL_ERROR "left behind after uninstall: ${d}")
    endif()
endforeach()
if(NOT EXISTS "${PREFIX}/foreign.txt")
    message(FATAL_ERROR "uninstall deleted a file it did not install (foreign.txt)")
endif()

message(STATUS "InstallerRoundTrip passed")
