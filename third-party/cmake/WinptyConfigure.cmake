if(NOT DEFINED PREFIX)
  message(FATAL_ERROR "PREFIX must be defined.")
endif()

if(NOT DEFINED CXX)
  message(FATAL_ERROR "CXX must be defined.")
endif()

if(NOT DEFINED TARGET)
  message(FATAL_ERROR "TARGET must be defined.")
endif()

set(CONFING_MK ${PREFIX}/src/${TARGET}/config.mk)

file(WRITE ${CONFING_MK} "MINGW_CXX=${CXX}\n")
file(APPEND ${CONFING_MK} "VERSION_SUFFIX ?= -dev\n")
file(APPEND ${CONFING_MK} "COMMIT_HASH ?= $$(git rev-parse HEAD)\n")
file(APPEND ${CONFING_MK} "BUILD_INFO_DEP = config.mk .git/HEAD")
