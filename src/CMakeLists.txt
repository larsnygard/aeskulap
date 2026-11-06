# aeskulap 0.2.9 — main application

set(aeskulap_SOURCES
  binreloc.c
  abusycursor.cpp
  astockids.cpp
  aiconfactory.cpp
  mainwindow.cpp
  studymanager.cpp
  settings.cpp
  prescandialog.cpp
  aboutdialog.cpp
  windowleveldialog.cpp
  main.cpp
  seriesmath.cpp
)

if(WIN32)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/aeskulap.rc.in ${CMAKE_CURRENT_BINARY_DIR}/aeskulap.rc @ONLY)
  list(APPEND aeskulap_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/aeskulap.rc)
endif()

add_executable(aeskulap ${aeskulap_SOURCES})
if(WIN32)
  set_target_properties(aeskulap PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_include_directories(aeskulap PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/widgets
  ${CMAKE_SOURCE_DIR}/imagepool
  ${CMAKE_SOURCE_DIR}/configuration
  ${CMAKE_SOURCE_DIR}
  ${GTKMM_INCLUDE_DIRS}
  ${DCMTK_INCLUDE_DIRS}
)

target_compile_options(aeskulap PRIVATE
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>
  $<$<CXX_COMPILER_ID:MSVC>:/W0 /utf-8>)

target_link_libraries(aeskulap PRIVATE
  aeskulap-widgets
  imagepool
  aeskulap-config
  ${GTKMM_LIBRARIES}
  ${DCMTK_LIBS}
  Threads::Threads
)

if(Intl_FOUND)
  target_link_libraries(aeskulap PRIVATE Intl::Intl)
endif()

# data: GtkBuilder UI + pixmaps icons
file(GLOB AESKULAP_PIXMAPS ${CMAKE_SOURCE_DIR}/pixmaps/*.png)
install(FILES ${CMAKE_SOURCE_DIR}/src/aeskulap.ui DESTINATION share/aeskulap/glade)
install(FILES ${AESKULAP_PIXMAPS} DESTINATION share/aeskulap/images)
install(TARGETS aeskulap RUNTIME DESTINATION bin)
