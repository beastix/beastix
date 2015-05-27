PROJECT (netscsid)
INCLUDE_DIRECTORIES(../include ../wodim ${CMAKE_BINARY_DIR} )
INCLUDE(../include/AddScgBits.cmake)
INCLUDE(../include/AddSchilyBits.cmake)
INCLUDE(../include/AddNetworkBits.cmake)

LINK_DIRECTORIES(../librols ../libusal )
ADD_EXECUTABLE (netscsid netscsid.c)
#SET_SOURCE_FILES_PROPERTIES(netscsid.c )
TARGET_LINK_LIBRARIES(netscsid ${EXTRA_LIBS} )
#SET_TARGET_PROPERTIES(netscsid PROPERTIES  SKIP_BUILD_RPATH TRUE)
INSTALL(TARGETS netscsid DESTINATION sbin)

