
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was IzekipUIConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

include(CMakeFindDependencyMacro)

# GLFW's own exported INTERFACE_LINK_LIBRARIES references Threads::Threads (found by
# actually running the end-to-end SDK verification: a consumer project that never calls
# find_package(Threads) itself hit a hard "target was not found" error processing
# IzekipUITargets.cmake without this).
find_dependency(Threads)

# Baked in at install time from this build's IZEKIP_RENDER_BACKEND cache variable (see
# cmake/Options.cmake) -- a consumer's find_package(IzekipUI) picks up whichever backend
# this particular install was built with; it does not re-select a backend.
set(IzekipUI_RENDER_BACKEND "D3D11")

if(IzekipUI_RENDER_BACKEND STREQUAL "VULKAN")
    find_dependency(Vulkan COMPONENTS dxc)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/IzekipUITargets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/IzekipUIAssets.cmake")

check_required_components(IzekipUI)
