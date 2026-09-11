add_library(xblob_core STATIC
  src/scheduler.cpp
)

target_include_directories(xblob_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(xblob_core
  PUBLIC
    xblob_common
)

xblob_set_target_warnings(xblob_core)
xblob_enable_sanitizers(xblob_core)
