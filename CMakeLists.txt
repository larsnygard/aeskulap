cmake_minimum_required(VERSION 3.22)
project(aeskulap VERSION 0.3.0 LANGUAGES C CXX)

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

# Link only the DCMTK modules the app includes (dcmdata, dcmimage, dcmimgle,
# dcmjpeg, dcmnet, dcmtls, ofstd/oflog). The pkg-config Libs line drags in the
# entire 28-module set (dcmrt, dcmtract, dcmpmap, ...) and with it libxml2,
# sndfile, openjp2 — pure dead weight for a viewer + PACS client.
set(_dcmtk_needed ofstd oflog oficonv dcmdata i2d dcmimgle dcmimage dcmjpeg ijg8 ijg12 ijg16 dcmjpls dcmtkcharls dcmtls dcmnet)
set(DCMTK_LIBS "")
foreach(_lib ${DCMTK_LIBRARIES})
  get_filename_component(_base "${_lib}" NAME_WE)   # libdcmdata.so -> libdcmdata
  string(REGEX REPLACE "^lib" "" _name "${_base}")
  if(_name IN_LIST _dcmtk_needed)
    list(APPEND DCMTK_LIBS "${_lib}")
  endif()
endforeach()
if(NOT DCMTK_LIBS)
  set(DCMTK_LIBS ${DCMTK_LIBRARIES})   # pkg-config gave bare -l names: keep all
endif()

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

# ---- headless logic tests (cross-platform; run with: ctest --test-dir build) ----
enable_testing()

function(aeskulap_logic_test name)
  add_executable(${name} ${ARGN})
  target_include_directories(${name} PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/widgets
    ${CMAKE_SOURCE_DIR}/imagepool ${CMAKE_SOURCE_DIR}/configuration
    ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/include
    ${GTKMM_INCLUDE_DIRS} ${DCMTK_INCLUDE_DIRS})
  target_compile_options(${name} PRIVATE
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w -include ${CMAKE_SOURCE_DIR}/imagepool/compat.h>
    $<$<CXX_COMPILER_ID:MSVC>:/W0 /utf-8>)
  target_link_libraries(${name} PRIVATE
    aeskulap-widgets imagepool aeskulap-config
    ${GTKMM_LIBRARIES} ${DCMTK_LIBS}
    Threads::Threads)
  if(Intl_FOUND)
    target_link_libraries(${name} PRIVATE Intl::Intl)
  endif()
  add_test(NAME ${name} COMMAND ${name})
endfunction()

aeskulap_logic_test(test_render tests/test_render.cpp)
aeskulap_logic_test(test_seriesmath tests/test_seriesmath.cpp src/seriesmath.cpp)
aeskulap_logic_test(test_cpr tests/test_cpr.cpp widgets/cpr.cpp)

# ---- CPack source package (git-tracked tree only; the vendored 2007 dcmtk/
# legacy copy and web/ node_modules must never ship in a release tarball) ----
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_PACKAGE_NAME "aeskulap")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_SOURCE_IGNORE_FILES
  "/build" "/\\.git" "/dcmtk/" "/web/" "/node_modules/"
  "/\\.github/" "\\.o$" "\\.so$" "CMakeCache" "CMakeFiles")
include(CPack)

# ---- packaging ----
install(FILES ${CMAKE_SOURCE_DIR}/src/aeskulap.desktop DESTINATION share/applications)
install(FILES ${CMAKE_SOURCE_DIR}/pixmaps/aeskulap-256.png DESTINATION share/icons/hicolor/256x256/apps RENAME aeskulap.png)
install(FILES ${CMAKE_SOURCE_DIR}/pixmaps/aeskulap.png DESTINATION share/icons/hicolor/64x64/apps RENAME aeskulap.png)
