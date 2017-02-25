include(CMakeParseArguments)

# BuildLibuv(TARGET targetname CONFIGURE_COMMAND ... BUILD_COMMAND ... INSTALL_COMMAND ...)
# Reusable function to build libuv, wraps ExternalProject_Add.
# Failing to pass a command argument will result in no command being run
function(Buildwinpty)
  cmake_parse_arguments(_winpty
    "BUILD_IN_SOURCE"
    "TARGET"
    "CONFIGURE_COMMAND;BUILD_COMMAND;INSTALL_COMMAND"
    ${ARGN})

  if(NOT _winpty_CONFIGURE_COMMAND AND NOT _winpty_BUILD_COMMAND
        AND NOT _winpty_INSTALL_COMMAND)
    message(FATAL_ERROR "Must pass at least one of CONFIGURE_COMMAND, BUILD_COMMAND, INSTALL_COMMAND")
  endif()
  if(NOT _winpty_TARGET)
    set(_winpty_TARGET "winpty")
  endif()

  ExternalProject_Add(${_winpty_TARGET}
    PREFIX ${DEPS_BUILD_DIR}
    GIT_REPOSITORY ${WINPTY_URL}
    GIT_TAG ${WINPTY_TAG}
    BUILD_IN_SOURCE ${_winpty_BUILD_IN_SOURCE}
    CONFIGURE_COMMAND "${_winpty_CONFIGURE_COMMAND}"
    BUILD_COMMAND "${_winpty_BUILD_COMMAND}"
    INSTALL_COMMAND "${_winpty_INSTALL_COMMAND}")
endfunction()

if(MINGW)
  # Native MinGW
  set(_winpty_TARGET "winpty")
  Buildwinpty(BUILD_IN_SOURCE
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E remove ${DEPS_BUILD_DIR}/src/winpty/src/unix-adapter/subdir.mk
      COMMAND ${CMAKE_COMMAND} -E touch ${DEPS_BUILD_DIR}/src/winpty/src/unix-adapter/subdir.mk
      COMMAND ${CMAKE_COMMAND} -DPREFIX=${DEPS_BUILD_DIR} -DCXX=${CMAKE_CXX_COMPILER} -DTARGET=${_winpty_TARGET} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/WinptyConfigure.cmake
    BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} -f Makefile
    INSTALL_COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPS_INSTALL_DIR}/lib
      COMMAND ${CMAKE_COMMAND} -E copy ${DEPS_BUILD_DIR}/src/winpty/build/winpty.lib ${DEPS_INSTALL_DIR}/lib
      COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPS_INSTALL_DIR}/bin
      COMMAND ${CMAKE_COMMAND} -E copy ${DEPS_BUILD_DIR}/src/winpty/build/winpty.dll ${DEPS_INSTALL_DIR}/bin
      COMMAND ${CMAKE_COMMAND} -E copy ${DEPS_BUILD_DIR}/src/winpty/build/winpty-agent.exe ${DEPS_INSTALL_DIR}/bin
      COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPS_INSTALL_DIR}/include
      COMMAND ${CMAKE_COMMAND} -E copy ${DEPS_BUILD_DIR}/src/winpty/src/include/winpty.h ${DEPS_INSTALL_DIR}/include
      COMMAND ${CMAKE_COMMAND} -E copy ${DEPS_BUILD_DIR}/src/winpty/src/include/winpty_constants.h ${DEPS_INSTALL_DIR}/include
    )

else()
  message(FATAL_ERROR "Trying to build winpty in an unsupported system ${CMAKE_SYSTEM_NAME}/${CMAKE_C_COMPILER_ID}")
endif()

list(APPEND THIRD_PARTY_DEPS winpty)
