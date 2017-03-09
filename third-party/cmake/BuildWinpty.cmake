include(CMakeParseArguments)

# BuildWinpty(CONFIGURE_COMMAND ... BUILD_COMMAND ... INSTALL_COMMAND ...)
# Reusable function to build winpty, wraps ExternalProject_Add.
# Failing to pass a command argument will result in no command being run
function(Buildwinpty)
  cmake_parse_arguments(_winpty
    ""
    ""
    "CONFIGURE_COMMAND;BUILD_COMMAND;INSTALL_COMMAND"
    ${ARGN})

  if(NOT _winpty_CONFIGURE_COMMAND AND NOT _winpty_BUILD_COMMAND
      AND NOT _winpty_INSTALL_COMMAND)
    message(FATAL_ERROR "Must pass at least one of CONFIGURE_COMMAND, BUILD_COMMAND, INSTALL_COMMAND")
  endif()

  ExternalProject_Add(winpty
    PREFIX ${DEPS_BUILD_DIR}
    URL ${WINPTY_URL}
    DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}/winpty
    DOWNLOAD_COMMAND ${CMAKE_COMMAND}
    -DPREFIX=${DEPS_BUILD_DIR}
    -DDOWNLOAD_DIR=${DEPS_DOWNLOAD_DIR}/winpty
    -DURL=${WINPTY_URL}
    -DEXPECTED_SHA256=${WINPTY_SHA256}
    -DTARGET=winpty
    -DUSE_EXISTING_SRC_DIR=${USE_EXISTING_SRC_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/DownloadAndExtractFile.cmake
    CONFIGURE_COMMAND ""
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND "${_winpty_CONFIGURE_COMMAND}"
    BUILD_COMMAND "${_winpty_BUILD_COMMAND}"
    INSTALL_COMMAND "${_winpty_INSTALL_COMMAND}")
endfunction()

if(WIN32)
  set(WINPTY_CONFIGURE_COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/WinptyCMakeLists.txt
    ${DEPS_BUILD_DIR}/src/winpty/CMakeLists.txt
    COMMAND ${CMAKE_COMMAND} ${DEPS_BUILD_DIR}/src/winpty
    -DCMAKE_INSTALL_PREFIX=${DEPS_INSTALL_DIR}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    "-DCMAKE_CXX_FLAGS:STRING=${CMAKE_C_COMPILER_ARG1}"
    -DCMAKE_GENERATOR=${CMAKE_GENERATOR})
  set(WINPTY_BUILD_COMMAND ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE})
  set(WINPTY_INSTALL_COMMAND ${CMAKE_COMMAND} --build . --target install --config ${CMAKE_BUILD_TYPE})
  Buildwinpty(CONFIGURE_COMMAND ${WINPTY_CONFIGURE_COMMAND}
    BUILD_COMMAND ${WINPTY_BUILD_COMMAND}
    INSTALL_COMMAND ${WINPTY_INSTALL_COMMAND})
else()
  message(FATAL_ERROR "Trying to build winpty in an unsupported system ${CMAKE_SYSTEM_NAME}/${CMAKE_C_COMPILER_ID}")
endif()

list(APPEND THIRD_PARTY_DEPS winpty)
