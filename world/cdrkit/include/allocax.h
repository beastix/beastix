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

/* @(#)allocax.h	1.3 03/07/15 Copyright 2002 J. Schilling */
/*
 *	Definitions for users of alloca()
 *
 *	Important: #include this directly after <mconfig.h>
 *	and before any other include file.
 *	See comment in _AIX part below.
 *
 *	Copyright (c) 2002 J. Schilling
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


#ifndef	_ALLOCAX_H
#define	_ALLOCAX_H

#ifdef __GNUC__
#	ifndef	alloca
#		define	alloca(s)	__builtin_alloca(s)
#	endif
#else
#	ifdef _MSC_VER
#		include <malloc.h>
#		define alloca _alloca
#	else
#		ifdef HAVE_ALLOCA_H
#			include <alloca.h>
#		else
#			ifdef _AIX
				/*
				 * Indent so pre-ANSI compilers will ignore it
				 *
				 * Some versions of AIX may require this to be
				 * first in the file and only preceded by
				 * comments and preprocessor directives/
				 */
				#pragma alloca
#			else
#				ifndef alloca
					/*
					 * predefined by HP cc +Olibcalls
					 */
#					ifdef	PROTOTYPES
						extern void *alloca();
#					else
						extern char *alloca();
#					endif
#				endif
#			endif
#		endif
#	endif
#endif


#endif	/* _ALLOCAX_H */
