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

/* $Id: mkzftree.c,v 1.18 2006/07/04 04:57:42 hpa Exp $ */
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
 * mkzffile.c
 *
 *	- Generate block-compression of files for use with
 *	  the "ZF" extension to the iso9660/RockRidge filesystem.
 *
 *	  The file compression technique used is the "deflate"
 *	  algorithm used by the zlib library; each block must have a
 *	  valid (12-byte) zlib header.  In addition, the file itself
 *	  has the following structure:
 *
 *	  Byte offset	iso9660 type	Contents
 *	    0		(8 bytes)	Magic number (37 E4 53 96 C9 DB D6 07)
 *	    8		7.3.1		Uncompressed file size
 *	   12		7.1.1		header_size >> 2 (currently 4)
 *	   13		7.1.1		log2(block_size)
 *	   14		(2 bytes)	Reserved, must be zero
 *
 * The header may get expanded in the future, at which point the
 * header size field will be used to increase the space for the
 * header.
 *
 * All implementations are required to support a block_size of 32K
 * (byte 13 == 15).
 *
 * Note that bytes 12 and 13 and the uncompressed length are also
 * present in the ZF record; THE TWO MUST BOTH BE CONSISTENT AND
 * CORRECT.
 *
 * Given the uncompressed size, block_size, and header_size:
 *
 *     Nblocks := ceil(size/block_size)
 *
 * After the header follow (nblock+1) 32-bit pointers, recorded as
 * iso9660 7.3.1 (littleendian); each indicate the byte offset (from
 * the start of the file) to one block and the first byte beyond the
 * end of the previous block; the first pointer thus point to the
 * start of the data area and the last pointer to the first byte
 * beyond it:
 *
 *     block_no := floor(byte_offset/block_size)
 *
 *     block_start := read_pointer_731( (header_size+block_no)*4 )
 *     block_end   := read_pointer_731( (header_size+block_no+1)*4 )
 *
 * The block data is compressed according to "zlib".
 */

#include "mkzftree.h"		/* Must be included first! */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include "version.h"

/* Command line options */
struct cmdline_options opt = {
  0,				/* Force compression */
  9,				/* Compression level */
  0,				/* Parallelism (0 = strictly serial) */
  0,				/* One filesystem only */
  0,				/* One directory only */
  1,				/* Create stub directories */
  0,				/* Root may be a file */
  0,				/* Be paranoid about metadata */
  default_verbosity,		/* Default verbosity */
  block_compress_file		/* Default transformation function */
};

/* Program name */
const char *program;

/* Long options */
#define OPTSTRING "fz:up:xXC:lLFvqV:hw"
#ifdef HAVE_GETOPT_LONG
const struct option long_options[] = {
  { "force",	             0,  0,  'f' },
  { "level",                 1,  0,  'z' },
  { "uncompress",            0,  0,  'u' },
  { "parallelism",           1,  0,  'p' },
  { "one-filesystem",        0,  0,  'x' },
  { "strict-one-filesystem", 0,  0,  'X' },
  { "crib-tree",             1,  0,  'C' },
  { "local",                 0,  0,  'l' },
  { "strict-local",          0,  0,  'L' },
  { "file",                  0,  0,  'F' },
  { "verbose",               0,  0,  'v' },
  { "quiet",                 0,  0,  'q' },
  { "verbosity",             1,  0,  'V' },
  { "help",                  0,  0,  'h' },
  { "version",               0,  0,  'w' },
  { 0, 0, 0, 0 }
};
#define LO(X) X
#else
#define getopt_long(C,V,O,L,I) getopt(C,V,O)
#define LO(X)
#endif
  
static void usage(enum verbosity level, int err)
{
  message(level,
	  "zisofs-tools " ZISOFS_TOOLS_VERSION "\n"
	  "Usage: %s [options] intree outtree\n"
	  LO("  --force                ")"  -f    Always compress, even if result is larger\n"
	  LO("  --level #              ")"  -z #  Set compression level (1-9)\n"
	  LO("  --uncompress           ")"  -u    Uncompress an already compressed tree\n"
	  LO("  --parallelism #        ")"  -p #  Process up to # files in parallel\n"
	  LO("  --one-filesystem       ")"  -x    Do not cross filesystem boundaries\n"
	  LO("  --strict-one-filesystem")"  -X    Same as -x, but don't create stubs dirs\n"
	  LO("  --crib-tree            ")"  -C    Steal \"crib\" files from an old tree\n"
	  LO("  --local                ")"  -l    Do not recurse into subdirectoires\n"
	  LO("  --strict-local         ")"  -L    Same as -l, but don't create stubs dirs\n"
	  LO("  --file                 ")"  -F    Operate possibly on a single file\n"
	  LO("  --sloppy               ")"  -s    Don't abort if metadata cannot be set\n"
	  LO("  --verbose              ")"  -v    Increase message verbosity\n"
	  LO("  --verbosity #          ")"  -V #  Set message verbosity to # (default = %d)\n"
	  LO("  --quiet                ")"  -q    No messages, not even errors (-V 0)\n"
	  LO("  --help                 ")"  -h    Display this message\n"
	  LO("  --version              ")"  -w    Display the program version\n"
	  ,program, (int)default_verbosity);
  exit(err);
}

static int opt_atoi(const char *str)
{
  char *endptr;
  long out;

  out = strtol(str, &endptr, 10);
  if ( *endptr )
    usage(vl_error, EX_USAGE);

  return (int)out;
}


int main(int argc, char *argv[])
{
  const char *in, *out, *crib = NULL;
  struct stat st;
  int optch, err;

  program = argv[0];

  while ( (optch = getopt_long(argc, argv, OPTSTRING, long_options, NULL))
	  != EOF ) {
    switch(optch) {
    case 'f':
      opt.force = 1;		/* Always compress */
      break;
    case 'z':
      opt.level = opt_atoi(optarg);
      if ( opt.level < 1 || opt.level > 9 ) {
	message(vl_error, "%s: invalid compression level: %d\n",
		program, optarg);
	exit(EX_USAGE);
      }
      break;
    case 'v':
      opt.verbosity++;
      break;
    case 'V':
      opt.verbosity = opt_atoi(optarg);
      break;
    case 'q':
      opt.verbosity = vl_quiet;
      break;
    case 'u':
      opt.munger = block_uncompress_file;
      break;
    case 'C':
      crib = optarg;
      break;
    case 'p':
      opt.parallel = opt_atoi(optarg);
      break;
    case 'x':
      opt.onefs = 1;  opt.do_mkdir = 1;
      break;
    case 'l':
      opt.onedir = 1; opt.do_mkdir = 1;
      break;
    case 'X':
      opt.onefs = 1;  opt.do_mkdir = 0;
      break;
    case 'L':
      opt.onedir = 1; opt.do_mkdir = 0;
      break;
    case 'F':
      opt.file_root = 1;
      break;
    case 's':
      opt.sloppy = 1;
      break;
    case 'h':
      usage(vl_quiet, 0);
      break;
    case 'w':
      message(vl_quiet, "zisofs-tools " ZISOFS_TOOLS_VERSION "\n");
      exit(0);
    default:
      usage(vl_error, EX_USAGE);
      break;
    }
  }

  if ( (argc-optind) != 2 )
    usage(vl_error, EX_USAGE);

  in  = argv[optind];		/* Input tree */
  out = argv[optind+1];		/* Output tree */

  umask(077);

  if ( opt.file_root ) {
    if ( lstat(in, &st) ) {
      message(vl_error, "%s: %s: %s\n", program, in, strerror(errno));
      exit(EX_NOINPUT);
    }

    err = munge_entry(in, out, crib, NULL);
  } else {
    /* Special case: we use stat() for the root, not lstat() */
    if ( stat(in, &st) ) {
      message(vl_error, "%s: %s: %s\n", program, in, strerror(errno));
      exit(EX_NOINPUT);
    }
    if ( !S_ISDIR(st.st_mode) ) {
      message(vl_error, "%s: %s: Not a directory\n", program, in);
      exit(EX_DATAERR);
    }
    
    err = munge_tree(in, out, crib);
  }    

  wait_for_all_workers();
    
  if ( err )
    exit(err);

  if ( !opt.file_root ) {
    if ( chown(out, st.st_uid, st.st_gid) && !opt.sloppy ) {
      message(vl_error, "%s: %s: %s", program, out, strerror(errno));
      err = EX_CANTCREAT;
    }
    if ( chmod(out, st.st_mode) && !opt.sloppy && !err ) {
      message(vl_error, "%s: %s: %s", program, out, strerror(errno));
      err = EX_CANTCREAT;
    }
    if ( copytime(out, &st) && !opt.sloppy && !err ) {
      message(vl_error, "%s: %s: %s", program, out, strerror(errno));
      err = EX_CANTCREAT;
    }
  }

  return err;
}
