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

/* @(#)stdxlib.h	1.6 01/07/15 Copyright 1996 J. Schilling */
/*
 *	Definitions for stdlib
 *
 *	Copyright (c) 1996 J. Schilling
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

#ifndef _STDXLIB_H
#define	_STDXLIB_H

#ifndef	_MCONFIG_H
#include <mconfig.h>
#endif

#ifdef	HAVE_STDLIB_H
#ifndef	_INCL_STDLIB_H
#include <stdlib.h>
#define	_INCL_STDLIB_H
#endif
#else

extern	char	*malloc();
extern	char	*realloc();

extern	double	atof();

#endif	/* HAVE_STDLIB_H */

#ifndef	EXIT_FAILURE
#define	EXIT_FAILURE	1
#endif
#ifndef	EXIT_SUCCESS
#define	EXIT_SUCCESS	0
#endif
#ifndef	RAND_MAX
#define	RAND_MAX	32767
#endif

#endif	/* _STDXLIB_H */
