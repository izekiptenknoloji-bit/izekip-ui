#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "lexbor::lexbor_static" for configuration "Debug"
set_property(TARGET lexbor::lexbor_static APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(lexbor::lexbor_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/lexbor_staticd.lib"
  )

list(APPEND _cmake_import_check_targets lexbor::lexbor_static )
list(APPEND _cmake_import_check_files_for_lexbor::lexbor_static "${_IMPORT_PREFIX}/lib/lexbor_staticd.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
