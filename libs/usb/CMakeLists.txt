add_library(xblob_usb STATIC
  src/ohci_controller.cpp
  src/ohci_traversal.cpp
)

target_include_directories(xblob_usb
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(xblob_usb
  PUBLIC
    xblob_common
)

xblob_set_target_warnings(xblob_usb)
xblob_enable_sanitizers(xblob_usb)
