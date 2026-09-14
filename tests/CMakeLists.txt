add_library(xblob_test_fixtures STATIC
  fixtures/synthetic_media.cpp
  fixtures/synthetic_xdvdfs.cpp
)

target_include_directories(xblob_test_fixtures
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(xblob_test_fixtures
  PUBLIC
    xblob_formats
    xblob_io
    xblob_common
)

xblob_set_target_warnings(xblob_test_fixtures)
xblob_enable_sanitizers(xblob_test_fixtures)

# Helper function to add a unit/integration test
function(xblob_add_test TEST_NAME TEST_SOURCE)
  add_executable(${TEST_NAME} ${TEST_SOURCE})
  target_include_directories(${TEST_NAME}
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/..
      ${CMAKE_CURRENT_SOURCE_DIR}
  )
  if(ARGN)
    target_link_libraries(${TEST_NAME} PRIVATE ${ARGN})
  endif()
  xblob_set_target_warnings(${TEST_NAME})
  xblob_enable_sanitizers(${TEST_NAME})
  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

xblob_add_test(test_common unit/test_common.cpp xblob_common)
xblob_add_test(test_bus unit/test_bus.cpp xblob_memory)
xblob_add_test(test_pci unit/test_pci.cpp xblob_pci xblob_bus)
xblob_add_test(test_gpu unit/test_gpu.cpp xblob_gpu xblob_pci xblob_bus)
xblob_add_test(test_usb unit/test_usb.cpp xblob_usb)
xblob_add_test(test_input unit/test_input.cpp xblob_input)
xblob_add_test(test_io unit/test_io.cpp xblob_io)
xblob_add_test(test_formats unit/test_formats.cpp xblob_test_fixtures)
xblob_add_test(test_vfs unit/test_vfs.cpp xblob_vfs xblob_test_fixtures)
xblob_add_test(test_memory unit/test_memory.cpp xblob_memory)
xblob_add_test(test_virtual_memory unit/test_virtual_memory.cpp xblob_cpu)
xblob_add_test(test_loader unit/test_loader.cpp xblob_loader xblob_test_fixtures)
xblob_add_test(test_machine_session unit/test_machine_session.cpp xblob_machine xblob_test_fixtures)
xblob_add_test(test_machine_gpu unit/test_machine_gpu.cpp xblob_machine xblob_test_fixtures)
xblob_add_test(test_machine_interactive unit/test_machine_interactive.cpp xblob_machine xblob_test_fixtures)
xblob_add_test(test_scheduler unit/test_scheduler.cpp xblob_core)
xblob_add_test(test_cpu unit/test_cpu.cpp xblob_cpu)
xblob_add_test(test_cpu_extended unit/test_cpu_extended.cpp xblob_cpu)
xblob_add_test(test_kernel unit/test_kernel.cpp xblob_kernel xblob_test_fixtures)
xblob_add_test(test_inspector_integration integration/test_inspector_integration.cpp xblob_test_fixtures)
xblob_add_test(test_machine_integration integration/test_machine_integration.cpp xblob_cpu xblob_core)
xblob_add_test(test_c_api unit/test_c_api.cpp xblob_c_api xblob_test_fixtures)
xblob_add_test(test_c_api_smoke unit/test_c_api_smoke.c xblob_c_api)
