add_library(xblob_cpu STATIC
  src/cpu.cpp
  src/registers.cpp
  src/instructions.cpp
  src/decoder.cpp
  src/executor_data.cpp
  src/executor_alu.cpp
  src/executor_stack.cpp
  src/executor_branch.cpp
  src/executor_interrupt.cpp
)

target_include_directories(xblob_cpu
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(xblob_cpu
  PUBLIC
    xblob_common
    xblob_memory
)

xblob_set_target_warnings(xblob_cpu)
xblob_enable_sanitizers(xblob_cpu)
