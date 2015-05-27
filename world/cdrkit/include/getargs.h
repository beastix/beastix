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

/* @(#)getargs.h	1.10 04/09/25 Copyright 1985 J. Schilling */
/*
 *	Definitions for getargs()/getallargs()/getfiles()
 *
 *	Copyright (c) 1985 J. Schilling
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

#ifndef	_GETARGS_H
#define	_GETARGS_H

#ifndef _MCONFIG_H
#include <mconfig.h>
#endif
#ifndef _PROTOTYP_H
#include <prototyp.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#define	NOARGS		  0	/* No more args			*/
#define	NOTAFLAG	  1	/* Not a flag type argument	*/
#define	BADFLAG		(-1)	/* Not a valid flag argument	*/
#define	BADFMT		(-2)	/* Error in format string	*/
#define	NOTAFILE	(-3)	/* Seems to be a flag type	*/

typedef	int	(*getargfun)(const void *, void *);

/*
 * Keep in sync with schily.h
 */
extern	int	getallargs(int *, char * const**, const char *, ...);
extern	int	getargs(int *, char * const**, const char *, ...);
extern	int	getfiles(int *, char * const**, const char *);

#ifdef	__cplusplus
}
#endif

#endif	/* _GETARGS_H */
