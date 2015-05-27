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

/* @(#)config.h	1.7 03/10/06 Copyright 1998-2003 Heiko Eissfeldt */
/*
 *	a central configuration file
 */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <mconfig.h>

#if	__STDC__-0 != 0 || (defined PROTOTYPES && defined STDC_HEADERS)
#define UINT_C(a)	(a##u)
#define ULONG_C(a)	(a##ul)
#define USHORT_C(a)	(a##uh)
#define CONCAT(a,b)	a##b
#else
#define UINT_C(a)	((unsigned) a)
#define ULONG_C(a)	((unsigned long) a)
#define USHORT_C(a)	((unsigned short) a)
#define CONCAT(a,b)	a/**/b
#endif

#include "lconfig.h"

/* temporary until a autoconf check is present */
#ifdef	__BEOS__
#define	HAVE_AREAS	1
#endif

#if defined HAVE_FORK && (defined (HAVE_SMMAP) || defined(HAVE_USGSHM) || defined(HAVE_DOSALLOCSHAREDMEM) || defined (HAVE_AREAS))
#define HAVE_FORK_AND_SHAREDMEM
#undef FIFO
#define FIFO
#else
#undef FIFO
#endif
#if	!defined	HAVE_MEMMOVE
#define	memmove(dst, src, size)	movebytes((src), (dst), (size))
#endif
