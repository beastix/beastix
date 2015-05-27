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

/* @(#)mytype.h	1.2 99/12/19 Copyright 1998,1999 Heiko Eissfeldt */
#if 4 == SIZEOF_LONG_INT
#define UINT4 long unsigned int
#define UINT4_C ULONG_C
#else
#if 4 == SIZEOF_INT
#define UINT4 unsigned int
#define UINT4_C UINT_C
#else
#if 4 == SIZEOF_SHORT_INT
#define UINT4 short unsigned int
#define UINT4_C USHORT_C
#else
error need an integer type with 32 bits, but do not know one!
#endif
#endif
#endif
#define TRUE 1
#define FALSE 0

#ifndef offset_of
#define offset_of(TYPE, MEMBER) ((size_t) ((TYPE *)0)->MEMBER)
#endif
#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

