PROJECT (LIBunls)
INCLUDE_DIRECTORIES(../include ../libhfs_iso ../wodim ${CMAKE_BINARY_DIR})
ADD_DEFINITIONS(-DHAVE_CONFIG_H)

    IF(HAVE_ICONV_H)
ADD_DEFINITIONS(-DUSE_ICONV)
    ENDIF(HAVE_ICONV_H)


SET(LIBunls_SRCS nls_base.c nls_config.c nls_cp10000.c nls_cp10006.c nls_cp10007.c nls_cp10029.c nls_cp10079.c nls_cp10081.c nls_cp1250.c nls_cp1251.c nls_cp437.c nls_cp737.c nls_cp775.c nls_cp850.c nls_cp852.c nls_cp855.c nls_cp857.c nls_cp860.c nls_cp861.c nls_cp862.c nls_cp863.c nls_cp864.c nls_cp865.c nls_cp866.c nls_cp869.c nls_cp874.c nls_file.c nls_iso8859-1.c nls_iso8859-14.c nls_iso8859-15.c nls_iso8859-2.c nls_iso8859-3.c nls_iso8859-4.c nls_iso8859-5.c nls_iso8859-6.c nls_iso8859-7.c nls_iso8859-8.c nls_iso8859-9.c nls_koi8-r.c nls_koi8-u.c nls_iconv.c) 
#SET_SOURCE_FILES_PROPERTIES(${LIBunls_SRCS} PROPERTIES )
ADD_LIBRARY (unls STATIC ${LIBunls_SRCS})
