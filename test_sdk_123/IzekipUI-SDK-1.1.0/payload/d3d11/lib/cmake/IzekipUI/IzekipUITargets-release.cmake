#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "IzekipUI::IzekipUI" for configuration "Release"
set_property(TARGET IzekipUI::IzekipUI APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(IzekipUI::IzekipUI PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/IzekipUI.lib"
  )

list(APPEND _cmake_import_check_targets IzekipUI::IzekipUI )
list(APPEND _cmake_import_check_files_for_IzekipUI::IzekipUI "${_IMPORT_PREFIX}/lib/IzekipUI.lib" )

# Import target "IzekipUI::glfw" for configuration "Release"
set_property(TARGET IzekipUI::glfw APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(IzekipUI::glfw PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/glfw3.lib"
  )

list(APPEND _cmake_import_check_targets IzekipUI::glfw )
list(APPEND _cmake_import_check_files_for_IzekipUI::glfw "${_IMPORT_PREFIX}/lib/glfw3.lib" )

# Import target "IzekipUI::yogacore" for configuration "Release"
set_property(TARGET IzekipUI::yogacore APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(IzekipUI::yogacore PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/yogacore.lib"
  )

list(APPEND _cmake_import_check_targets IzekipUI::yogacore )
list(APPEND _cmake_import_check_files_for_IzekipUI::yogacore "${_IMPORT_PREFIX}/lib/yogacore.lib" )

# Import target "IzekipUI::lexbor_static" for configuration "Release"
set_property(TARGET IzekipUI::lexbor_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(IzekipUI::lexbor_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/lexbor_static.lib"
  )

list(APPEND _cmake_import_check_targets IzekipUI::lexbor_static )
list(APPEND _cmake_import_check_files_for_IzekipUI::lexbor_static "${_IMPORT_PREFIX}/lib/lexbor_static.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
