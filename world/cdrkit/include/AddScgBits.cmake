
IF(NOT GUARD_SCGCONFIG)
   SET(GUARD_SCGCONFIG 1)


LIST(APPEND EXTRA_LIBS "usal")

INCLUDE(CheckIncludeFiles)
CHECK_INCLUDE_FILES("stdio.h;camlib.h" HAVE_CAMLIB_H)

IF(HAVE_CAMLIB_H)

   # quick an dirty, should better become a variable used by libusal only,
   # analogous to SCG_SELF_LIBS
   ADD_DEFINITIONS(-DHAVE_CAMLIB_H)

   LIST(APPEND EXTRA_LIBS "cam")
   LIST(APPEND SCG_SELF_LIBS "cam")

ENDIF(HAVE_CAMLIB_H)

FIND_LIBRARY(HAVE_LIBVOLMGT "volmgt")
IF(HAVE_LIBVOLMGT)
   LIST(APPEND EXTRA_LIBS "volmgt")
   LIST(APPEND SCG_SELF_LIBS "volmgt")
ENDIF(HAVE_LIBVOLMGT)

   INCLUDE(CheckCSourceCompiles)

   SET(TESTSRC "
#include <sys/types.h>
#include <sys/socket.h>

int main(int argc, char **argv) {
   return socket(AF_INET, SOCK_STREAM, 0);
}
")

SET(CMAKE_REQUIRED_LIBRARIES )
   CHECK_C_SOURCE_COMPILES("${TESTSRC}" LIBC_SOCKET)

IF(NOT LIBC_SOCKET)
   LIST(APPEND EXTRA_LIBS -lsocket)
   #MESSAGE("Using libsocket for socket functions")
ENDIF(NOT LIBC_SOCKET)


   SET(TESTSRC "
#include <sched.h>
struct sched_param scp;
         int main(int argc, char **argv) {
         return sched_setscheduler(0, SCHED_RR, &scp);
         }
")


SET(CMAKE_REQUIRED_LIBRARIES )
   CHECK_C_SOURCE_COMPILES("${TESTSRC}" LIBC_SCHED)

IF(NOT LIBC_SCHED)
   LIST(APPEND EXTRA_LIBS -lrt)
   #MESSAGE("Using librt for realtime functions")
ENDIF(NOT LIBC_SCHED)

ENDIF(NOT GUARD_SCGCONFIG)
