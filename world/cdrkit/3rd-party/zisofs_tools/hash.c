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

#ident "$Id: hash.c,v 1.3 2006/07/04 04:57:42 hpa Exp $"
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

/*
 * hash.c
 *
 * Hash table used to find hard-linked files
 */

#include "mkzftree.h"		/* Must be included first! */

#define HASH_BUCKETS 	  2683

struct file_hash {
  struct file_hash *next;
  struct stat st;
  const char *outfile_name;
};

static struct file_hash *hashp[HASH_BUCKETS];

const char *hash_find_file(struct stat *st)
{
  int bucket = (st->st_ino + st->st_dev) % HASH_BUCKETS;
  struct file_hash *hp;

  for ( hp = hashp[bucket] ; hp ; hp = hp->next ) {
    if ( hp->st.st_ino   == st->st_ino &&
	 hp->st.st_dev   == st->st_dev &&
	 hp->st.st_mode  == st->st_mode &&
	 hp->st.st_nlink == st->st_nlink &&
	 hp->st.st_uid   == st->st_uid &&
	 hp->st.st_gid   == st->st_gid &&
	 hp->st.st_size  == st->st_size &&
	 hp->st.st_mtime == st->st_mtime ) {
      /* Good enough, it's the same file */
      return hp->outfile_name;
    }
  }
  return NULL;			/* No match */
}

/* Note: the stat structure is the input file; the name
   is the output file to link to */
void hash_insert_file(struct stat *st, const char *outfile)
{
  int bucket = (st->st_ino + st->st_dev) % HASH_BUCKETS;
  struct file_hash *hp = xmalloc(sizeof(struct file_hash));

  hp->next         = hashp[bucket];
  memcpy(&hp->st, st, sizeof(struct stat));
  hp->outfile_name = xstrdup(outfile);

  hashp[bucket]    = hp;
}


