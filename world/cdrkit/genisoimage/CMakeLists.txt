PROJECT (MKISOFS)

INCLUDE(../include/AddScgBits.cmake)
INCLUDE(../include/AddSchilyBits.cmake)

INCLUDE_DIRECTORIES(../include ../libhfs_iso ../wodim ${CMAKE_BINARY_DIR})

INCLUDE(CheckIncludeFiles)
CHECK_INCLUDE_FILES("magic.h" USE_MAGIC)
IF(USE_MAGIC)
	ADD_DEFINITIONS(-DUSE_MAGIC)
	SET(MAGICLIBS magic)
ENDIF(USE_MAGIC)

    IF(HAVE_ICONV_H)
    ADD_DEFINITIONS(-DUSE_ICONV)
    #INCLUDE(CheckLibraryExists)
#    CHECK_LIBRARY_EXISTS (iconv "iconv_open(argv[0],argv[0])" "" USE_LIBICONV)
IF(USE_LIBICONV)
    LIST(APPEND EXTRA_LIBS "iconv")
ELSE(USE_LIBICONV)
#           MESSAGE("No additional libiconv found, maybe not required. If required, change linker flags, see TROUBLESHOOTING in the INSTALL file")
ENDIF(USE_LIBICONV)
ENDIF(HAVE_ICONV_H)

ADD_DEFINITIONS(-DUSE_LARGEFILES -DABORT_DEEP_ISO_ONLY -DAPPLE_HYB -DUDF -DDVD_VIDEO -DSORTING -DHAVE_CONFIG_H -DUSE_LIBSCHILY -DUSE_SCG -DJIGDO_TEMPLATE)

SET(MKISOFS_MOST_SRCS apple.c boot.c desktop.c dvd_file.c dvd_reader.c eltorito.c exclude.c files.c fnmatch.c hash.c ifo_read.c joliet.c mac_label.c match.c multi.c name.c rock.c scsi.c stream.c tree.c udf.c vms.c volume.c write.c boot-alpha.c boot-hppa.c boot-mips.c md5.c jte.c rsync.c
 	boot-mipsel.c endian.c )
  
INCLUDE(CheckFunctionExists)
SET(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
CHECK_FUNCTION_EXISTS("getopt_long" HAVE_GETOPT_LONG)
IF(NOT HAVE_GETOPT_LONG)
    LIST ( APPEND MKISOFS_MOST_SRCS  getopt.c getopt1.c )
ENDIF(NOT HAVE_GETOPT_LONG)
 

#SET_SOURCE_FILES_PROPERTIES(${MKISOFS_MOST_SRCS} PROPERTIES )
LINK_DIRECTORIES(../libhfs_iso ../librols ../libusal ../libunls  ../wodim)

ADD_EXECUTABLE (genisoimage genisoimage.c ${MKISOFS_MOST_SRCS})
TARGET_LINK_LIBRARIES(genisoimage wodimstuff hfs_iso  ${MAGICLIBS} c unls z ${EXTRA_LIBICONV} ${EXTRA_LIBS})

# common lib set and genisoimage source parts for the rest
LINK_LIBRARIES(wodimstuff ${EXTRA_LIBS}  unls ${EXTRA_LIBICONV} )
ADD_EXECUTABLE(devdump diag/dump.c scsi.c)
ADD_EXECUTABLE(isodebug	diag/isodebug.c scsi.c)
ADD_EXECUTABLE(isodump	diag/isodump.c scsi.c)
ADD_EXECUTABLE(isoinfo diag/isoinfo.c scsi.c)
ADD_EXECUTABLE(isovfy	diag/isovfy.c scsi.c)


SET_TARGET_PROPERTIES(genisoimage devdump isodebug isodump isoinfo isovfy PROPERTIES  SKIP_BUILD_RPATH TRUE)

INSTALL(TARGETS genisoimage devdump isodebug isodump isoinfo isovfy DESTINATION bin)
INSTALL(FILES 
genisoimage.1
diag/devdump.1
diag/isodebug.1
diag/isodump.1
diag/isoinfo.1
diag/isovfy.1
DESTINATION ${MANSUBDIR}/man1)
INSTALL(FILES genisoimagerc.5 DESTINATION "${MANSUBDIR}/man5")
