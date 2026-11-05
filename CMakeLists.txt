cmake_minimum_required(VERSION 3.22)
project(aeskulap VERSION 0.2.9 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)
pkg_check_modules(DCMTK REQUIRED dcmtk)

# gettext for translations (po/)
find_package(Intl)
if(Intl_FOUND)
  set(ENABLE_NLS 1)
endif()

configure_file(${CMAKE_SOURCE_DIR}/config.h.cmake ${CMAKE_BINARY_DIR}/include/config.h)

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(JPEG REQUIRED)
find_package(TIFF REQUIRED)
find_package(PNG REQUIRED)
find_package(OpenSSL REQUIRED)

add_subdirectory(imagepool)
add_subdirectory(configuration)
add_subdirectory(widgets)
add_subdirectory(src)
