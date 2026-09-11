add_library(xblob_memory STATIC
  src/ram.cpp
  src/address_space.cpp
  src/mmio.cpp
  src/bus_adapter.cpp
  src/virtual_memory.cpp
)

target_include_directories(xblob_memory
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(xblob_memory
  PUBLIC
    xblob_common
    xblob_bus
)

xblob_set_target_warnings(xblob_memory)
xblob_enable_sanitizers(xblob_memory)
