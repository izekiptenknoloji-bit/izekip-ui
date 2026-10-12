# Debian package (.deb) of the SDK, built with CPack from the same install() rules as cmake --install.
# Only for Linux and only when IzekipUI is the top-level project. Build it with a /usr prefix so the
# files land where Debian packages put them:
#   cmake -S . -B build-linux -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release ...
#   cmake --build build-linux
#   cd build-linux && cpack -G DEB

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    return()
endif()

set(CPACK_PACKAGE_NAME "izekip-ui-sdk")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "İzekip Teknoloji")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "İzekip UI: CSS ile stillenen C++ GUI kütüphanesi (SDK)")
set(CPACK_PACKAGE_DESCRIPTION "Headers, static libraries, the cssfr scaffolding tool and runtime assets of İzekip UI.")
set(CPACK_PACKAGE_CONTACT "İzekip Teknoloji <izekiptenknoloji@gmail.com>")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${PROJECT_VERSION}_${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_GENERATOR "DEB")

set(CPACK_DEBIAN_PACKAGE_SECTION "libdevel")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "İzekip Teknoloji <izekiptenknoloji@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libvulkan1, libx11-6, libxrandr2, libxinerama1, libxcursor1, libxi6, libwayland-client0, libxkbcommon0")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "mesa-vulkan-drivers")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF) # the libraries are static; the deps above are the runtime ones

include(CPack)
