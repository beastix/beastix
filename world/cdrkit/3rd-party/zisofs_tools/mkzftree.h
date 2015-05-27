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

#ident "$Id: mkzftree.h,v 1.7 2006/07/04 04:57:42 hpa Exp $"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2001-2006 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 *   USA; either version 2 of the License, or (at your option) any later
 *   version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#ifndef MKZFTREE_H
#define MKZFTREE_H

/* config.h should be included before any system headers!!!! */
#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#ifdef HAVE_ENDIAN_H
#include <endian.h>
#endif

#ifdef HAVE_SYSEXITS_H
#include <sysexits.h>
#else
#define EX_USAGE	64	/* command line usage error */
#define EX_DATAERR	65	/* data format error */
#define EX_NOINPUT	66	/* cannot open input */
#define EX_NOUSER	67	/* addressee unknown */
#define EX_NOHOST	68	/* host name unknown */
#define EX_UNAVAILABLE	69	/* service unavailable */
#define EX_SOFTWARE	70	/* internal software error */
#define EX_OSERR	71	/* system error (e.g., can't fork) */
#define EX_OSFILE	72	/* critical OS file missing */
#define EX_CANTCREAT	73	/* can't create (user) output file */
#define EX_IOERR	74	/* input/output error */
#define EX_TEMPFAIL	75	/* temp failure; user is invited to retry */
#define EX_PROTOCOL	76	/* remote error in protocol */
#define EX_NOPERM	77	/* permission denied */
#define EX_CONFIG	78	/* configuration error */
#endif

/* File transformation functions */  
typedef int (*munger_func)(FILE *, FILE *, off_t);
int block_compress_file(FILE *, FILE *, off_t);
int block_uncompress_file(FILE *, FILE *, off_t);

/* mkzftree.c */
extern const char *program;	/* Program name */
enum verbosity {		/* Message verbosity */
  vl_quiet,			/* No messages */
  vl_error,			/* Error messages only */
  vl_filename,			/* Display filenames */
  vl_crib,			/* Cribbing files */
};
#define default_verbosity vl_error
struct cmdline_options {
  int force;			/* Always compress */
  int level;			/* Compression level */
  int parallel;			/* Parallelism (0 = strictly serial) */
  int onefs;			/* One filesystem only */
  int onedir;			/* One directory only */
  int do_mkdir;			/* Create stub directories */
  int file_root;		/* The root may be a file */
  int sloppy;			/* Don't make sure metadata is set correctly */
  enum verbosity verbosity;	/* Message verbosity */
  munger_func munger;		/* Default action */
};
extern struct cmdline_options opt;

/* walk.c */
int munge_tree(const char *, const char *, const char *);
int munge_entry(const char *, const char *, const char *, const struct stat *);

/* workers.c */
void wait_for_all_workers(void);
int spawn_worker(void);
void end_worker(int);

/* util.c */
void *xmalloc(size_t);
void *xrealloc(void *, size_t);
char *xstrdup(const char *);
void message(enum verbosity, const char *, ...);

/* hash.c */
const char *hash_find_file(struct stat *);
void hash_insert_file(struct stat *, const char *);

/* copytime.h */
int copytime(const char *, const struct stat *);

#endif /* MKZFTREE_H */
