add_library(xblob_c_api STATIC
  src/c_api.cpp
  src/c_api_machine.cpp
)

target_include_directories(xblob_c_api
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(xblob_c_api
  PUBLIC
    xblob_machine
    xblob_formats
    xblob_io
    xblob_common
)

xblob_set_target_warnings(xblob_c_api)
xblob_enable_sanitizers(xblob_c_api)

install(TARGETS xblob_c_api
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)
