PROJECT (LIBROLS)
INCLUDE_DIRECTORIES(../include ../libhfs_iso ${CMAKE_BINARY_DIR} ../wodim)

SET(LIBROLS_SRCS  astoi.c astoll.c astoull.c breakline.c cmpbytes.c comerr.c error.c fexec.c fillbytes.c getargs.c getav0.c getdomainname.c geterrno.c gethostid.c gethostname.c getpagesize.c handlecond.c movebytes.c raisecond.c rename.c saveargs.c seterrno.c spawn.c strcatl.c strdup.c streql.c swabbytes.c usleep.c stdio/cvmod.c stdio/dat.c stdio/fcons.c stdio/fdown.c stdio/fdup.c stdio/ffileread.c stdio/ffilewrite.c stdio/fgetline.c stdio/file_raise.c stdio/fileclose.c stdio/fileluopen.c stdio/filemopen.c stdio/fileopen.c  stdio/filepos.c stdio/fileread.c stdio/filereopen.c stdio/fileseek.c stdio/filesize.c stdio/filestat.c stdio/filewrite.c stdio/flag.c stdio/flush.c stdio/fpipe.c stdio/niread.c stdio/niwrite.c stdio/nixread.c stdio/nixwrite.c stdio/openfd.c stdio/peekc.c default.c)

ADD_LIBRARY (rols STATIC ${CMAKE_CURRENT_SOURCE_DIR}/../include/xconfig.h  ${CMAKE_CURRENT_SOURCE_DIR}/../include/align.h ${LIBROLS_SRCS})

