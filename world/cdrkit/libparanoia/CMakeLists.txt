PROJECT (LIBparanoia)
INCLUDE_DIRECTORIES(../include ${CMAKE_BINARY_DIR} ../wodim)
ADD_DEFINITIONS(-DHAVE_CONFIG_H)
SET(LIBparanoia_SRCS gap.c isort.c overlap.c p_block.c paranoia.c pmalloc.c)

ADD_LIBRARY (paranoia STATIC ${LIBparanoia_SRCS})
