add_library(xblob_io STATIC
  src/file_source.cpp
  src/binary_reader.cpp
)

target_include_directories(xblob_io
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(xblob_io
  PUBLIC
    xblob_common
)

xblob_set_target_warnings(xblob_io)
xblob_enable_sanitizers(xblob_io)

install(TARGETS xblob_io
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)
