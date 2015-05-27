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

/* @(#)sha.h	1.4 03/06/28 Copyright 1998,1999 Heiko Eissfeldt */
/*____________________________________________________________________________
//
//   CD Index - The Internet CD Index
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2 of the License, or
//   (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software
//   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
//   $Id: sha.h,v 1.1.1.2 1999/04/29 00:53:34 marc Exp $
//____________________________________________________________________________
*/
#ifndef SHA_H
#define SHA_H

/* NIST Secure Hash Algorithm */
/* heavily modified by Uwe Hollerbach <uh@alumni.caltech edu> */
/* from Peter C. Gutmann's implementation as found in */
/* Applied Cryptography by Bruce Schneier */

/* This code is in the public domain */

/* Useful defines & typedefs */

typedef unsigned char BYTE;	/* 8-bit quantity */
typedef unsigned long ULONG;	/* 32-or-more-bit quantity */

#define SHA_BLOCKSIZE		64
#define SHA_DIGESTSIZE		20

typedef struct {
    ULONG digest[5];		/* message digest */
    ULONG count_lo, count_hi;	/* 64-bit bit count */
    BYTE data[SHA_BLOCKSIZE];	/* SHA data buffer */
    int local;			/* unprocessed amount in data */
} SHA_INFO;

void sha_init(SHA_INFO *);
void sha_update(SHA_INFO *, BYTE *, int);
void sha_final(unsigned char [20], SHA_INFO *);

#ifdef SHA_FOR_C

#include <mconfig.h>
#include <stdxlib.h>
#include <stdio.h>

void sha_stream(unsigned char [20], SHA_INFO *, FILE *);
void sha_print(unsigned char [20]);
char *sha_version(void);
#endif /* SHA_FOR_C */

#define SHA_VERSION 1

#ifndef WIN32 
#ifdef WORDS_BIGENDIAN
#  if SIZEOF_UNSIGNED_LONG_INT == 4
#    define SHA_BYTE_ORDER  4321
#  else
#    if SIZEOF_UNSIGNED_LONG_INT == 8
#      define SHA_BYTE_ORDER  87654321
#    endif
#  endif
#else
#  if SIZEOF_UNSIGNED_LONG_INT == 4
#    define SHA_BYTE_ORDER  1234
#  else
#    if SIZEOF_UNSIGNED_LONG_INT == 8
#      define SHA_BYTE_ORDER  12345678
#    endif
#  endif
#endif

#else

#define SHA_BYTE_ORDER 1234

#endif

#endif /* SHA_H */
