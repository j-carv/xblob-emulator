add_library(xblob_machine STATIC
  src/machine_session.cpp
  src/media_boot_pipeline.cpp
  src/trace_buffer.cpp
)

target_include_directories(xblob_machine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(xblob_machine
  PUBLIC
    xblob_common
    xblob_core
    xblob_cpu
    xblob_kernel
    xblob_memory
    xblob_bus
    xblob_pci
    xblob_gpu
    xblob_loader
    xblob_formats
    xblob_vfs
)

xblob_set_target_warnings(xblob_machine)
xblob_enable_sanitizers(xblob_machine)
