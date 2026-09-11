add_library(xblob_bus STATIC
  src/bus.cpp
  src/synthetic_register_bank.cpp
)

target_include_directories(xblob_bus
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(xblob_bus
  PUBLIC
    xblob_common
)

xblob_set_target_warnings(xblob_bus)
xblob_enable_sanitizers(xblob_bus)
