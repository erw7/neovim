include(CMakeParseArguments)

# BuildLibuv(TARGET targetname CONFIGURE_COMMAND ... BUILD_COMMAND ... INSTALL_COMMAND ...)
# Reusable function to build libuv, wraps ExternalProject_Add.
# Failing to pass a command argument will result in no command being run
function(BuildLibuv)
  cmake_parse_arguments(_libuv
    ""
    ""
    "CONFIGURE_COMMAND;BUILD_COMMAND;INSTALL_COMMAND"
    ${ARGN})

  if(NOT _libuv_CONFIGURE_COMMAND AND NOT _libuv_BUILD_COMMAND AND NOT _libuv_INSTALL_COMMAND)
    message(FATAL_ERROR "Must pass at least one of CONFIGURE_COMMAND, BUILD_COMMAND, INSTALL_COMMAND")
  endif()
  if(NOT _libuv_TARGET)
    set(_libuv_TARGET "libuv")
  endif()

  ExternalProject_Add(${_libuv_TARGET}
    PREFIX ${DEPS_BUILD_DIR}
    URL ${LIBUV_URL}
    DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}/libuv
    DOWNLOAD_COMMAND ${CMAKE_COMMAND}
      -DPREFIX=${DEPS_BUILD_DIR}
      -DDOWNLOAD_DIR=${DEPS_DOWNLOAD_DIR}/libuv
      -DURL=${LIBUV_URL}
      -DEXPECTED_SHA256=${LIBUV_SHA256}
      -DTARGET=${_libuv_TARGET}
      -DUSE_EXISTING_SRC_DIR=${USE_EXISTING_SRC_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/DownloadAndExtractFile.cmake
    CONFIGURE_COMMAND "${_libuv_CONFIGURE_COMMAND}"
    BUILD_COMMAND "${_libuv_BUILD_COMMAND}"
    INSTALL_COMMAND "${_libuv_INSTALL_COMMAND}")
endfunction()

set(LIBUV_CONFIGURE_COMMAND ${CMAKE_COMMAND} ${DEPS_BUILD_DIR}/src/libuv
  -DCMAKE_INSTALL_PREFIX=${DEPS_INSTALL_DIR}
  -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
  -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  -DCMAKE_GENERATOR=${CMAKE_GENERATOR}
  -DBUILD_SHARED_LIBS=${BUILD_SHARED}
  -DLIBUV_BUILD_TESTS=OFF)

if(UNIX)
  list(APPEND LIBUV_CONFIGURE_COMMAND
    "-DCMAKE_C_FLAGS:STRING=${CMAKE_C_COMPILER_ARG1} -fPIC")
  list(APPEND LIBUV_CONFIGURE_COMMAND
    -DCMAKE_INSTALL_LIBDIR=${DEPS_INSTALL_DIR}/lib)
endif()

set(LIBUV_BUILD_COMMAND ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE})
if(MSVC)
  # Since building luv-static fails, remove the static library.
  set(LIBUV_INSTALL_COMMAND ${CMAKE_COMMAND}
    --build . --target install --config ${CMAKE_BUILD_TYPE}
    COMMAND ${CMAKE_COMMAND} -E remove ${DEPS_INSTALL_DIR}/lib/uv.lib
    COMMAND ${CMAKE_COMMAND} -E rename
      ${DEPS_INSTALL_DIR}/lib/uv_import.lib ${DEPS_INSTALL_DIR}/lib/uv.lib)
else()
  set(LIBUV_INSTALL_COMMAND ${CMAKE_COMMAND} --build . --target install --config ${CMAKE_BUILD_TYPE})
endif()

if(MINGW AND CMAKE_CROSSCOMPILING)
  get_filename_component(TOOLCHAIN ${CMAKE_TOOLCHAIN_FILE} REALPATH)
  set(LIBUV_CONFIGURE_COMMAND ${CMAKE_COMMAND} ${DEPS_BUILD_DIR}/src/libuv
    -DCMAKE_INSTALL_PREFIX=${DEPS_INSTALL_DIR}
    # Pass toolchain
    -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    # Hack to avoid -rdynamic in Mingw
    -DCMAKE_SHARED_LIBRARY_LINK_C_FLAGS="")
endif()

BuildLibuv(CONFIGURE_COMMAND ${LIBUV_CONFIGURE_COMMAND}
  BUILD_COMMAND ${LIBUV_BUILD_COMMAND}
  INSTALL_COMMAND ${LIBUV_INSTALL_COMMAND})

list(APPEND THIRD_PARTY_DEPS libuv)
