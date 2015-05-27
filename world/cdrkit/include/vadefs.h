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

/* @(#)vadefs.h	1.5 01/07/15 Copyright 1998 J. Schilling */
/*
 *	Generic header for users of var args ...
 *
 *	Includes a default definition for va_copy()
 *	and some magic know how about the SVr4 Power PC var args ABI
 *	to create a __va_arg_list() macro.
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

#ifndef	_VADEFS_H
#define	_VADEFS_H

#ifndef	_MCONFIG_H
#include <mconfig.h>
#endif

#ifdef	PROTOTYPES
/*
 * For ANSI C-compilers prefer stdarg.h
 */
#	ifdef	HAVE_STDARG_H
#		ifndef	_INCL_STDARG_H
#		include <stdarg.h>
#		define	_INCL_STDARG_H
#		endif
#	else
#		ifndef	_INCL_VARARGS_H
#		include <varargs.h>
#		define	_INCL_VARARGS_H
#		endif
#	endif
#else
/*
 * For K&R C-compilers prefer varargs.h
 */
#	ifdef	HAVE_VARARGS_H
#		ifndef	_INCL_VARARGS_H
#		include <varargs.h>
#		define	_INCL_VARARGS_H
#		endif
#	else
#		ifndef	_INCL_STDARG_H
#		include <stdarg.h>
#		define	_INCL_STDARG_H
#		endif
#	endif
#endif

#if (defined(__linux__) || defined(__linux) || defined(sun)) && \
		(defined(__ppc) || defined(__PPC) || defined(powerpc) || defined(__powerpc__))

#	ifndef	VA_LIST_IS_ARRAY
#	define	VA_LIST_IS_ARRAY
#	endif
#endif


/*
 * __va_copy() is used by GCC 2.8 or newer until va_copy() becomes
 * a final ISO standard.
 */
#if !defined(va_copy) && !defined(HAVE_VA_COPY)
#	if	defined(__va_copy)
#		define	va_copy(to, from)	__va_copy(to, from)
#	endif
#endif

/*
 * va_copy() is a Solaris extension to provide a portable way to perform a
 * variable argument list "bookmarking" function.
 * If it is not available via stdarg.h, use a simple assignement for backward
 * compatibility.
 */
#if !defined(va_copy) && !defined(HAVE_VA_COPY)
#ifdef	VA_LIST_IS_ARRAY
#	define	va_copy(to, from)	((to)[0] = (from)[0])
#else
#	define	va_copy(to, from)	((to) = (from))
#endif
#endif

/*
 * I don't know any portable way to get an arbitrary
 * C object from a var arg list so I use a
 * system-specific routine __va_arg_list() that knows
 * if 'va_list' is an array. You will not be able to
 * assign the value of __va_arg_list() but it works
 * to be used as an argument of a function.
 * It is a requirement for recursive printf to be able
 * to use this function argument. If your system
 * defines va_list to be an array you need to know this
 * via autoconf or another mechanism.
 * It would be nice to have something like
 * __va_arg_list() in stdarg.h
 */

#ifdef	VA_LIST_IS_ARRAY
#	define	__va_arg_list(list)	va_arg(list, void *)
#else
#	define	__va_arg_list(list)	va_arg(list, va_list)
#endif

#endif	/* _VADEFS_H */
