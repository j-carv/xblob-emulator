add_library(xblob_kernel STATIC
  src/registry.cpp
  src/debug_sink.cpp
  src/guest_heap.cpp
  src/sync_objects.cpp
  src/thread_scheduler.cpp
  src/file_services.cpp
  src/kernel_hle.cpp
)

target_include_directories(xblob_kernel
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(xblob_kernel
  PUBLIC
    xblob_common
    xblob_memory
    xblob_cpu
    xblob_core
    xblob_vfs
)

xblob_set_target_warnings(xblob_kernel)
xblob_enable_sanitizers(xblob_kernel)
