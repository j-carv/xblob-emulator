cmake_minimum_required(VERSION 3.22)

project(
  xblob
  VERSION 0.1.0
  DESCRIPTION "Emulador Xbox clássico modular e de longo prazo"
  LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)

option(XBLOB_BUILD_TESTS "Construir suíte de testes do xblob" ON)
option(XBLOB_BUILD_APPS "Construir aplicações CLI do xblob" ON)

add_subdirectory(libs/common)
add_subdirectory(libs/bus)
add_subdirectory(libs/pci)
add_subdirectory(libs/io)
add_subdirectory(libs/formats)
add_subdirectory(libs/vfs)
add_subdirectory(libs/memory)
add_subdirectory(libs/core)
add_subdirectory(libs/cpu)
add_subdirectory(libs/gpu)
add_subdirectory(libs/usb)
add_subdirectory(libs/input)
add_subdirectory(libs/kernel)
add_subdirectory(libs/loader)
add_subdirectory(libs/machine)
add_subdirectory(libs/c_api)

if(XBLOB_BUILD_APPS)
  add_subdirectory(apps/inspect)
endif()

if(XBLOB_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
