file(GLOB WIDGETS_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(aeskulap-widgets STATIC ${WIDGETS_SOURCES})

target_include_directories(aeskulap-widgets PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/imagepool
  ${CMAKE_SOURCE_DIR}/configuration
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_BINARY_DIR}/include
  ${DCMTK_INCLUDE_DIRS}
  ${GTKMM_INCLUDE_DIRS}
)

target_compile_options(aeskulap-widgets PRIVATE
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w -include ${CMAKE_SOURCE_DIR}/imagepool/compat.h>
  $<$<CXX_COMPILER_ID:MSVC>:/W0 /utf-8>)

target_link_libraries(aeskulap-widgets PUBLIC
  imagepool aeskulap-config
  ${GTKMM_LIBRARIES} ${DCMTK_LIBS}
)
