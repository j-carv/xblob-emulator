# cmake/Sanitizers.cmake

function(xblob_enable_sanitizers TARGET_NAME)
  option(XBLOB_ENABLE_SANITIZERS "Habilitar AddressSanitizer e UndefinedBehaviorSanitizer" OFF)

  if(NOT XBLOB_ENABLE_SANITIZERS)
    return()
  endif()

  if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(SAN_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_compile_options(${TARGET_NAME} PRIVATE ${SAN_FLAGS})
    target_link_options(${TARGET_NAME} PRIVATE ${SAN_FLAGS})
    message(STATUS "Sanitizers (ASan/UBSan) habilitados para ${TARGET_NAME}")
  elseif(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /fsanitize=address)
    message(STATUS "AddressSanitizer (MSVC) habilitado para ${TARGET_NAME}")
  else()
    message(WARNING "Sanitizers não suportados para o compilador ${CMAKE_CXX_COMPILER_ID}")
  endif()
endfunction()
