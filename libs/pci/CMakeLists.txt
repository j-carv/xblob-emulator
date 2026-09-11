add_library(xblob_pci STATIC
  src/pci_bar.cpp
  src/pci_config_header.cpp
  src/pci_registry.cpp
  src/pci_bus_bridge.cpp
)

target_include_directories(xblob_pci
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(xblob_pci
  PUBLIC
    xblob_common
    xblob_bus
)

xblob_set_target_warnings(xblob_pci)
xblob_enable_sanitizers(xblob_pci)

install(TARGETS xblob_pci
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)
