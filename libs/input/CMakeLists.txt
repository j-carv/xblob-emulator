add_library(xblob_input STATIC
  src/xid_controller.cpp
)

target_include_directories(xblob_input
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(xblob_input
  PUBLIC
    xblob_usb
    xblob_common
)

xblob_set_target_warnings(xblob_input)
xblob_enable_sanitizers(xblob_input)
