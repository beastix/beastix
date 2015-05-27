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

#ident "$Id: iso9660.h,v 1.4 2006/07/04 04:57:42 hpa Exp $"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2001 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 *   USA; either version 2 of the License, or (at your option) any later
 *   version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/* zisofs definitions */

#ifndef ISO9660_H
#define ISO9660_H

#ifndef CBLOCK_SIZE_LG2
#define CBLOCK_SIZE_LG2	15	/* Compressed block size */
#endif
#define CBLOCK_SIZE	(1 << CBLOCK_SIZE_LG2)

/* Compressed file magic */
extern const unsigned char zisofs_magic[8];

/* VERY VERY VERY IMPORTANT: Must be a multiple of 4 bytes */
struct compressed_file_header {
  char magic[8];
  char uncompressed_len[4];
  unsigned char header_size;
  unsigned char block_size;
  char reserved[2];		/* Reserved for future use, MBZ */
};

/* iso9660 integer formats */
void set_721(void *, unsigned int);
unsigned int get_721(void *);
void set_722(void *, unsigned int);
unsigned int get_722(void *);
void set_723(void *, unsigned int);
void set_731(void *, unsigned int);
unsigned int get_731(void *);
void set_732(void *, unsigned int);
unsigned int get_732(void *);
void set_733(void *, unsigned int);
#define get_723(x) get_721(x)
#define get_733(x) get_731(x)

#endif /* ISO9660_H */
