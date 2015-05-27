PROJECT (CDRECORD)
INCLUDE_DIRECTORIES(../include ../libedc ${CMAKE_BINARY_DIR})
INCLUDE(../include/AddScgBits.cmake)
include(../include/AddSchilyBits.cmake)

#AUX_SOURCE_DIRECTORY(. CDRECORD_SRCS)
SET(CDRECORD_SRCS wodim.c audiosize.c auinfo.c  cdr_drv.c cdtext.c clone.c crc16.c cue.c  diskid.c drv_7501.c drv_jvc.c drv_mmc.c drv_philips.c drv_simul.c drv_sony.c fifo.c  isosize.c scsi_cdr_mmc4.c scsi_mmc4.c sector.c subchan.c wm_packet.c wm_session.c wm_track.c xio.c)
SET(CDRECORD_COMMON_SRCS cd_misc.c defaults.c getnum.c misc.c modes.c movesect.c scsi_cdr.c scsi_mmc.c scsi_scan.c)

INCLUDE(CheckIncludeFiles)

#force libcap usage on Linux
CHECK_INCLUDE_FILES("sys/capability.h" HAVE_SYS_CAPABILITY_H)
IF(HAVE_SYS_CAPABILITY_H)
	LIST(APPEND EXTRA_LIBS cap)
ELSE(HAVE_SYS_CAPABILITY_H)
   IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
      MESSAGE(FATAL_ERROR "Error: found a Linux system but no libcap header. Install libcap-dev.")
   ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")
ENDIF(HAVE_SYS_CAPABILITY_H)

LINK_DIRECTORIES(../librols ../libusal  ../libedc)

ADD_DEFINITIONS(-DHAVE_LIB_EDC_ECC -DCLONE_WRITE -DDRV_DVD -DFIFO -DAUINFO -DUSE_LARGEFILES )


ADD_EXECUTABLE (wodim ${CDRECORD_SRCS})
ADD_LIBRARY (wodimstuff STATIC ${CDRECORD_COMMON_SRCS})
LIST(APPEND EXTRA_LIBS wodimstuff)

TARGET_LINK_LIBRARIES(wodim ${EXTRA_LIBS}  edc)
SET_TARGET_PROPERTIES(wodim PROPERTIES SKIP_BUILD_RPATH TRUE)

INSTALL(TARGETS wodim DESTINATION bin)
INSTALL(FILES 
   wodim.1
DESTINATION ${MANSUBDIR}/man1)
