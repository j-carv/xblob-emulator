add_library(xblob_vfs
    src/vfs_path.cpp
    src/vfs_handle_table.cpp
    src/vfs.cpp
)

target_include_directories(xblob_vfs
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(xblob_vfs
    PUBLIC
        xblob_common
        xblob_io
        xblob_formats
)

target_compile_features(xblob_vfs PUBLIC cxx_std_20)

xblob_set_target_warnings(xblob_vfs)
xblob_enable_sanitizers(xblob_vfs)

install(TARGETS xblob_vfs
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)

if(BUILD_SHARED_LIBS)
    set_target_properties(xblob_vfs PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
