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

/* $Id: util.c,v 1.3 2006/07/04 04:57:42 hpa Exp $ */
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

#include "mkzftree.h"		/* Must be included first! */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/* Convenience functions */
void *xmalloc(size_t size)
{
  void *p = malloc(size);

  if ( !p ) {
    perror(program);
    exit(EX_OSERR);
  }

  return p;
}

char *xstrdup(const char *str)
{
  char *s = strdup(str);

  if ( !s ) {
    perror(program);
    exit(EX_OSERR);
  }
  
  return s;
}

void message(enum verbosity level, const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  if ( opt.verbosity >= level )
    vfprintf(stderr, format, ap);
  va_end(ap);
}

