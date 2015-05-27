PROJECT (INCLUDES)

FILE(READ "${CMAKE_SOURCE_DIR}/VERSION" CDRKIT_VERSION)
STRING(REPLACE "\n" "" CDRKIT_VERSION "${CDRKIT_VERSION}")
MESSAGE("Configuring Cdrkit, version ${CDRKIT_VERSION}")

# auto-generated header with sizeof magic
INCLUDE(CheckTypeSize)
CHECK_TYPE_SIZE(short SIZE_SHORT)
MATH(EXPR SHORTLESS ${SIZE_SHORT}-1)
CHECK_TYPE_SIZE(int SIZE_INT)
MATH(EXPR INTLESS ${SIZE_INT}-1)
CHECK_TYPE_SIZE(long SIZE_LONG)
MATH(EXPR LONGLESS ${SIZE_LONG}-1)
CHECK_TYPE_SIZE("long long" SIZE_LLONG)
MATH(EXPR LLONGLESS ${SIZE_LLONG}-1)
CHECK_TYPE_SIZE(float SIZE_FLOAT)
MATH(EXPR FLOATLESS ${SIZE_FLOAT}-1)
CHECK_TYPE_SIZE(double SIZE_DOUBLE)
MATH(EXPR DOUBLELESS ${SIZE_DOUBLE}-1)
CHECK_TYPE_SIZE("char*" SIZE_PTR)
MATH(EXPR PTRLESS ${SIZE_PTR}-1)
CONFIGURE_FILE( align.h.in align.h)
#SET_SOURCE_FILES_PROPERTIES(align.h PROPERTIES GENERATED 1)

TRY_COMPILE(VA_LIST_IS_ARRAY ${CMAKE_BINARY_DIR}  ${CMAKE_CURRENT_SOURCE_DIR}/test_VA_LIST_IS_ARRAY.c)
# invert the result, has the condition IFF compiler fails. There must be a simple way to say a=!b... but how?
IF(VA_LIST_IS_ARRAY) 
   SET(VA_LIST_IS_ARRAY FALSE)
else(VA_LIST_IS_ARRAY) 
   SET(VA_LIST_IS_ARRAY TRUE)
endif(VA_LIST_IS_ARRAY)


INCLUDE(TestBigEndian)
TEST_BIG_ENDIAN(WORDS_BIGENDIAN)

TRY_RUN(BITFIELDS_HTOL TEST_DUMMY ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/test_BITFIELDS_HTOL.c)

INCLUDE(CheckIncludeFiles)

#SET(CMAKE_REQUIRED_INCLUDES "/usr/include;/usr/local/include")
CHECK_INCLUDE_FILES("stdarg.h" HAVE_STDARG_H)
CHECK_INCLUDE_FILES("stdlib.h" HAVE_STDLIB_H)
CHECK_INCLUDE_FILES("string.h" HAVE_STRING_H)
CHECK_INCLUDE_FILES("strings.h" HAVE_STRINGS_H)
CHECK_INCLUDE_FILES("unistd.h" HAVE_UNISTD_H)
CHECK_INCLUDE_FILES("getopt.h" HAVE_GETOPT_H)
CHECK_INCLUDE_FILES("limits.h" HAVE_LIMITS_H)
CHECK_INCLUDE_FILES("fcntl.h" HAVE_FCNTL_H)
CHECK_INCLUDE_FILES("sys/file.h" HAVE_SYS_FILE_H)
CHECK_INCLUDE_FILES("inttypes.h" HAVE_INTTYPES_H)
CHECK_INCLUDE_FILES("dirent.h" HAVE_DIRENT_H)
CHECK_INCLUDE_FILES("alloca.h" HAVE_ALLOCA_H)
CHECK_INCLUDE_FILES("termios.h" HAVE_TERMIOS_H)
CHECK_INCLUDE_FILES("termio.h" HAVE_TERMIO_H)
CHECK_INCLUDE_FILES("pwd.h" HAVE_PWD_H)
CHECK_INCLUDE_FILES("sys/time.h" HAVE_SYS_TIME_H)
CHECK_INCLUDE_FILES("utime.h" HAVE_UTIME_H)
CHECK_INCLUDE_FILES("sys/ioctl.h" HAVE_SYS_IOCTL_H)
CHECK_INCLUDE_FILES("sys/param.h" HAVE_SYS_PARAM_H)
CHECK_INCLUDE_FILES("wait.h" HAVE_WAIT_H)
CHECK_INCLUDE_FILES("sys/wait.h" HAVE_SYS_WAIT_H)
CHECK_INCLUDE_FILES("sys/resource.h" HAVE_SYS_RESOURCE_H)
CHECK_INCLUDE_FILES("sys/utsname.h" HAVE_SYS_UTSNAME_H)
CHECK_INCLUDE_FILES("poll.h" HAVE_POLL_H)
CHECK_INCLUDE_FILES("sys/poll.h" HAVE_SYS_POLL_H)
CHECK_INCLUDE_FILES("netdb.h" HAVE_NETDB_H)
CHECK_INCLUDE_FILES("sys/socket.h" HAVE_SYS_SOCKET_H)
CHECK_INCLUDE_FILES("linux/pg.h" HAVE_LINUX_PG_H)
CHECK_INCLUDE_FILES("arpa/inet.h" HAVE_ARPA_INET_H)
CHECK_INCLUDE_FILES("sys/ipc.h" HAVE_SYS_IPC_H)
CHECK_INCLUDE_FILES("sys/types.h;sys/mman.h" HAVE_SYS_MMAN_H)
CHECK_INCLUDE_FILES("sys/types.h;sys/dkio.h" HAVE_SYS_DKIO_H)
CHECK_INCLUDE_FILES("sys/types.h;sun/dkio.h" HAVE_SUN_DKIO_H)
CHECK_INCLUDE_FILES("iconv.h" HAVE_ICONV_H)


INCLUDE(CheckCSourceCompiles)
INCLUDE(CheckFunctionExists)

#SET(CMAKE_REQUIRED_LIBRARIES iconv)
SET(TESTSRC " 
#include <iconv.h>
int main(int a, char **b) { iconv_t foo = iconv_open(b[0], b[0]); return(0); }
")

CHECK_C_SOURCE_COMPILES("${TESTSRC}" USE_LIBC_ICONV)
IF(NOT USE_LIBC_ICONV)
SET(CMAKE_REQUIRED_LIBRARIES iconv)
CHECK_C_SOURCE_COMPILES("${TESTSRC}" USE_LIBICONV)
IF(NOT USE_LIBICONV)
   message("ERROR: no library providing iconv found")
ENDIF(NOT USE_LIBICONV)
ENDIF(NOT USE_LIBC_ICONV)
SET(CMAKE_REQUIRED_INCLUDES )



#CHECK_C_SOURCE_COMPILES("${TESTSRC}" USE_LIBICONV)
#IF(NOT USE_LIBICONV)
#   FIND_FILE(ICONV_H_LOCATION "iconv.h")
#   IF(NOT "${ICONV_H_LOCATION}" EQUAL "/usr/include/iconv.h")
#      MESSAGE("WARNING: iconv.h found in non-standard location (${ICONV_H_LOCATION}) but")
#      MESSAGE("GNU libiconv is not available. See INSTALL for details. Sleeping 10 seconds.")
#      EXECUTE_PROCESS(COMMAND "sleep" "10")
#   ENDIF(NOT "${ICONV_H_LOCATION}" EQUAL "/usr/include/iconv.h")
#ENDIF(NOT USE_LIBICONV)

SET(CMAKE_REQUIRED_LIBRARIES )
SET(TESTSRC " 
#include <stdlib.h>
int main() { return ( (int) drand48()  )  ; }
")
CHECK_C_SOURCE_COMPILES("${TESTSRC}" HAVE_DRAND48)
SET(TESTSRC " 
#include <stdlib.h>
int main() { return ( (int) drand()  )  ; }
")
CHECK_C_SOURCE_COMPILES("${TESTSRC}" HAVE_DRAND)
SET(TESTSRC " 
#include <sys/socket.h>
#include <netdb.h>
int main(int argc, char **argv) {
struct sockaddr sa;
return getnameinfo(&sa, 1, argv[0], 1, argv[1], 1, 0);
}
")
CHECK_C_SOURCE_COMPILES("${TESTSRC}" HAVE_GETNAMEINFO)
SET(TESTSRC " 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
int main(int a, char **b) {
struct in_addr in;
inet_ntoa(in);
return 0;
}
")
CHECK_C_SOURCE_COMPILES("${TESTSRC}" HAVE_INET_NTOA)

SET(TESTSRC "
#include <sys/types.h>
#include <wait.h>
int main() {
union wait w;
return 0; 
}
")
# FIXME: try the same with #include <sys/wait.h>... may work, if yes, integrate properly
CHECK_C_SOURCE_COMPILES("${TESTSRC}" HAVE_UNION_WAIT)
SET(TESTSRC "
#include <sys/types.h>
#include <sys/mman.h>
int main() {
if (mlockall(MCL_CURRENT|MCL_FUTURE) < 0)  
   return(0);
   }
")
CHECK_C_SOURCE_COMPILES("${TESTSRC}" HAVE_MLOCKALL)
SET(TESTSRC "
int main(int argc, char **argv) {
   int a[argc];
   return 0;
}
")
CHECK_C_SOURCE_COMPILES("${TESTSRC}" HAVE_DYN_ARRAYS)

CONFIGURE_FILE( xconfig.h.in xconfig.h)

