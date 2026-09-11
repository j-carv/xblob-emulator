add_library(xblob_loader STATIC
  src/xbe_loader.cpp
)

target_include_directories(xblob_loader
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(xblob_loader
  PUBLIC
    xblob_common
    xblob_formats
    xblob_memory
    xblob_cpu
)

xblob_set_target_warnings(xblob_loader)
xblob_enable_sanitizers(xblob_loader)
