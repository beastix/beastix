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

/* $Id: copytime.c,v 1.2 2006/07/04 04:57:42 hpa Exp $ */
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2004 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Bostom MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * copytime.c
 *
 * Copy time(s) from a struct stat
 */

#include "mkzftree.h"		/* Must be included first! */

#include <utime.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

int copytime(const char *filename, const struct stat *st)
{
#if defined(HAVE_UTIMES) && (defined(HAVE_STRUCT_STAT_ST_MTIM_TV_USEC) || \
			     defined(HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC))
  
  struct timeval tv[2];
  
# ifdef HAVE_STRUCT_STAT_ST_MTIM_TV_USEC
  tv[0] = st->st_atim;
  tv[1] = st->st_mtim;
# else
  tv[0].tv_sec  = st->st_atim.tv_sec;
  tv[0].tv_usec = st->st_atim.tv_nsec / 1000;
  tv[1].tv_sec  = st->st_mtim.tv_sec;
  tv[1].tv_usec = st->st_mtim.tv_nsec / 1000;
# endif
  return utimes(filename, tv);
  
#else
  
  struct utimbuf ut; 
  
  ut.actime  = st->st_atime;
  ut.modtime = st->st_mtime;
  return utime(filename, &ut);
  
#endif
}
