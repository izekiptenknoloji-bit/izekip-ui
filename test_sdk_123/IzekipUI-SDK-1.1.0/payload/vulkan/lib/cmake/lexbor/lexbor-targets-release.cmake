#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "lexbor::lexbor_static" for configuration "Release"
set_property(TARGET lexbor::lexbor_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(lexbor::lexbor_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/lexbor_static.lib"
  )

list(APPEND _cmake_import_check_targets lexbor::lexbor_static )
list(APPEND _cmake_import_check_files_for_lexbor::lexbor_static "${_IMPORT_PREFIX}/lib/lexbor_static.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
