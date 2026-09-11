add_library(xblob_common STATIC
  src/error.cpp
  src/safe_math.cpp
)

target_include_directories(xblob_common
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

xblob_set_target_warnings(xblob_common)
xblob_enable_sanitizers(xblob_common)

install(TARGETS xblob_common
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)
