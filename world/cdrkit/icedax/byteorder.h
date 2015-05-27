/*
 * This file has been modified for the cdrkit suite.
 *
 * The behaviour and appearence of the program code below can differ to a major
 * extent from the version distributed by the original author(s).
 *
 * For details, see Changelog file distributed with the cdrkit package. If you
 * received this file from another source then ask the distributing person for
 * a log of modifications.
 *
 */

/* @(#)byteorder.h	1.3 03/07/20 Copyright 1998,1999 Heiko Eissfeldt */
#ifndef MYBYTE_ORDER
#define MYBYTE_ORDER 1
/* supply the byte order macros */

#if defined (WORDS_BIGENDIAN)
# define MY_BIG_ENDIAN 1
# define MY_LITTLE_ENDIAN 0
#else
# define MY_BIG_ENDIAN 0
# define MY_LITTLE_ENDIAN 1
#endif

# undef cpu_to_le32
# undef cpu_to_le16
# undef cpu_to_be32
# undef cpu_to_be16
# undef le32_to_cpu
# undef le16_to_cpu
# undef be32_to_cpu
# undef be16_to_cpu

#  define revert4bytes(x) \
        ((unsigned long int)((((unsigned long int)(x) & ULONG_C(0x000000ff)) << 24) | \
                             (((unsigned long int)(x) & ULONG_C(0x0000ff00)) <<  8) | \
                             (((unsigned long int)(x) & ULONG_C(0x00ff0000)) >>  8) | \
                             (((unsigned long int)(x) & ULONG_C(0xff000000)) >> 24)))
#  define revert2bytes(x) \
        ((unsigned short int)((((unsigned short int)(x) & 0x00ff) <<  8) | \
                              (((unsigned short int)(x) & 0xff00) >>  8)))

#if    MY_BIG_ENDIAN == 1
#  define cpu_to_le32(x) revert4bytes(x)
#  define cpu_to_le16(x) revert2bytes(x)
#  define le32_to_cpu(x) cpu_to_le32(x)
#  define le16_to_cpu(x) cpu_to_le16(x)
#  define cpu_to_be32(x) (x)
#  define cpu_to_be16(x) (x)
#  define be32_to_cpu(x) (x)
#  define be16_to_cpu(x) (x)
#else
#  define cpu_to_be32(x) revert4bytes(x)
#  define cpu_to_be16(x) revert2bytes(x)
#  define be32_to_cpu(x) cpu_to_be32(x)
#  define be16_to_cpu(x) cpu_to_be16(x)
#  define cpu_to_le32(x) (x)
#  define cpu_to_le16(x) (x)
#  define le32_to_cpu(x) (x)
#  define le16_to_cpu(x) (x)
#endif

#define GET_LE_UINT_FROM_CHARP(p) ((unsigned int)((*(p+3))<<24)|((*(p+2))<<16)|((*(p+1))<<8)|(*(p)))
#define GET_BE_UINT_FROM_CHARP(p) ((unsigned int)((*(p))<<24)|((*(p+1))<<16)|((*(p+2))<<8)|(*(p+3)))

#endif /* ifndef MYBYTE_ORDER */
