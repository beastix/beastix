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

/* @(#)getpagesize.c	1.1 01/11/28 Copyright 2001 J. Schilling */
/*
 *	Copyright (c) 2001 J. Schilling
 */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING.  If not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <mconfig.h>

#ifndef	HAVE_GETPAGESIZE

#include <unixstd.h>
#include <standard.h>
#ifdef	HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <libport.h>

#ifdef	HAVE_OS_H
#include <OS.h>		/* BeOS for B_PAGE_SIZE */
#endif

int getpagesize(void)
{
#ifdef _SC_PAGESIZE
	return sysconf(_SC_PAGESIZE);
#else
# ifdef PAGESIZE		/* Traditional UNIX page size from param.h */
	return PAGESIZE;
# else

#  ifdef B_PAGE_SIZE		/* BeOS page size from OS.h */
	return B_PAGE_SIZE;
#  else
	return 512;
#  endif
# endif
#endif
}

#endif	/* HAVE_GETPAGESIZE */
