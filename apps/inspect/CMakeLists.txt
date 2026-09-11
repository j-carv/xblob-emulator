add_executable(xblob-inspect
  main.cpp
)

target_link_libraries(xblob-inspect
  PRIVATE
    xblob_formats
    xblob_io
    xblob_common
)

xblob_set_target_warnings(xblob-inspect)
xblob_enable_sanitizers(xblob-inspect)
