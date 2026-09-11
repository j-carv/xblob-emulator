add_library(xblob_gpu STATIC
  src/gpu_surface.cpp
  src/gpu_register_file.cpp
  src/nv2a_device.cpp
  src/pushbuffer_decoder.cpp
  src/pushbuffer_processor.cpp
)

target_include_directories(xblob_gpu
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(xblob_gpu
  PUBLIC
    xblob_common
    xblob_bus
    xblob_pci
)

xblob_set_target_warnings(xblob_gpu)
xblob_enable_sanitizers(xblob_gpu)

install(TARGETS xblob_gpu
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)
