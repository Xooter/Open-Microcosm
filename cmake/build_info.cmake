# Genera build_info.hpp con la versión, el commit y los flags (se corre en cada build y solo
# reescribe el archivo si cambió, así no recompila de más).
execute_process(COMMAND git describe --always --dirty --abbrev=12
                WORKING_DIRECTORY ${SRC}
                OUTPUT_VARIABLE BIO_GIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT BIO_GIT)
    set(BIO_GIT "desconocido")
endif()
set(content "#pragma once
// Generado por cmake/build_info.cmake: no editar.
#define BIO_VERSION \"${VERSION}\"
#define BIO_GIT \"${BIO_GIT}\"
#define BIO_BUILD_FLAGS \"${FLAGS}\"
")
if(EXISTS ${OUT})
    file(READ ${OUT} old)
endif()
if(NOT "${old}" STREQUAL "${content}")
    file(WRITE ${OUT} "${content}")
endif()
