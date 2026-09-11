add_library(xblob_formats STATIC
  src/xbe.cpp
  src/xiso.cpp
  src/inspector.cpp
)

target_include_directories(xblob_formats
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(xblob_formats
  PUBLIC
    xblob_io
    xblob_common
)

xblob_set_target_warnings(xblob_formats)
xblob_enable_sanitizers(xblob_formats)

install(TARGETS xblob_formats
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)
