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

/* @(#)mmapdefs.h	1.1 01/02/25 Copyright 2001 J. Schilling */
/*
 *	Definitions to be used for mmap()
 *
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

#ifndef	_MMAPDEFS_H
#define	_MMAPDEFS_H

#ifndef	_MCONFIG_H
#include <mconfig.h>
#endif

#ifndef	_INCL_SYS_TYPES_H
#include <sys/types.h>
#define	_INCL_SYS_TYPES_H
#endif

#if defined(HAVE_SMMAP)

#ifndef	_INCL_SYS_MMAN_H
#include <sys/mman.h>
#define	_INCL_SYS_MMAN_H
#endif

#ifndef	MAP_ANONYMOUS
#	ifdef	MAP_ANON
#	define	MAP_ANONYMOUS	MAP_ANON
#	endif
#endif

#ifndef MAP_FILE
#	define MAP_FILE		0	/* Needed on Apollo Domain/OS */
#endif

/*
 * Needed for Apollo Domain/OS and may be for others?
 */
#ifdef	_MMAP_WITH_SIZEP
#	define	mmap_sizeparm(s)	(&(s))
#else
#	define	mmap_sizeparm(s)	(s)
#endif

#endif	/* defined(HAVE_SMMAP) */

#endif	/* _MMAPDEFS_H */
