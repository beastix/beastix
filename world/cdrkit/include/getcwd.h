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

/* @(#)getcwd.h	1.3 01/07/15 Copyright 1998 J. Schilling */
/*
 *	Definitions for getcwd()
 *
 *	Copyright (c) 1998 J. Schilling
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

#ifndef	_GETCWD_H
#define	_GETCWD_H

#ifndef _MCONFIG_H
#include <mconfig.h>
#endif

#ifdef JOS
#	ifndef	_INCL_SYS_STYPES_H
#	include <sys/stypes.h>
#	define	_INCL_SYS_STYPES_H
#	endif
	extern char	*gwd();
#	define		getcwd(b, len)	gwd(b)
#else
#	ifndef	HAVE_GETCWD
#		define	getcwd(b, len)	getwd(b)
#	endif
#endif

#endif	/* _GETCWD_H */
