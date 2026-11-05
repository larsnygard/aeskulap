add_library(imagepool STATIC
  imagepool.cpp
  loader.cpp
  fileloader.cpp
  dicomdir.cpp
  dicomdirloader.cpp
  netloader.cpp
  netquery.cpp
  poolnetwork.cpp
  poolassociation.cpp
  poolfindassociation.cpp
  poolmoveassociation.cpp
  poolinstance.cpp
  poolstudy.cpp
  poolseries.cpp
  poolservers.cpp
)

target_include_directories(imagepool PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/configuration
  ${CMAKE_SOURCE_DIR}          # gettext.h
  ${DCMTK_INCLUDE_DIRS}
  ${GTKMM_INCLUDE_DIRS}
)

target_compile_options(imagepool PRIVATE -w -include ${CMAKE_CURRENT_SOURCE_DIR}/compat.h)

target_include_directories(imagepool PUBLIC ${CMAKE_BINARY_DIR}/include)

target_link_libraries(imagepool PUBLIC
  ${DCMTK_LIBRARIES}
  ${GTKMM_LIBRARIES}
  JPEG::JPEG TIFF::TIFF PNG::PNG ZLIB::ZLIB OpenSSL::Crypto OpenSSL::SSL
  Threads::Threads
)
