add_library(aeskulap-config STATIC
  aconfiguration.cpp
  aconfigclient.cpp
)

target_include_directories(aeskulap-config PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}          # gettext.h
  ${CMAKE_BINARY_DIR}/include  # generated config.h
  ${GTKMM_INCLUDE_DIRS}
)

target_compile_options(aeskulap-config PRIVATE -w)

target_link_libraries(aeskulap-config PUBLIC ${GTKMM_LIBRARIES})
