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

/* @(#)getav0.c	1.16 04/05/09 Copyright 1985, 1995-2004 J. Schilling */
/*
 *	Get arg vector by scanning the stack
 *
 *	Copyright (c) 1985, 1995-2004 J. Schilling
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
#include <sigblk.h>
#include <avoffset.h>
#include <standard.h>
#include <schily.h>

#if	!defined(AV_OFFSET) || !defined(FP_INDIR)
#	ifdef	HAVE_SCANSTACK
#	undef	HAVE_SCANSTACK
#	endif
#endif

#ifdef	HAVE_SCANSTACK

#include <stkframe.h>

#define	is_even(p)	((((long)(p)) & 1) == 0)
#define	even(p)		(((long)(p)) & ~1L)
#ifdef	__future__
#define	even(p)		(((long)(p)) - 1) /* will this work with 64 bit ?? */
#endif

EXPORT	char	**getmainfp	__PR((void));
EXPORT	char	**getavp	__PR((void));
EXPORT	char	*getav0		__PR((void));


EXPORT char **
getmainfp()
{
	register struct frame *fp;
		char	**av;
#if	FP_INDIR > 0
	register int	i = 0;
#endif

	/*
	 * As the SCO OpenServer C-Compiler has a bug that may cause
	 * the first function call to getfp() been done before the
	 * new stack frame is created, we call getfp() twice.
	 */
	(void) getfp();
	fp = (struct frame *)getfp();
	if (fp == NULL)
		return (NULL);

	while (fp->fr_savfp) {
		if (fp->fr_savpc == NULL)
			break;

		if (!is_even(fp->fr_savfp)) {
			fp = (struct frame *)even(fp->fr_savfp);
			if (fp == NULL)
				break;
			fp = (struct frame *)((SIGBLK *)fp)->sb_savfp;
			continue;
		}
		fp = (struct frame *)fp->fr_savfp;

#if	FP_INDIR > 0
		i++;
#endif
	}

#if	FP_INDIR > 0
	i -= FP_INDIR;
	fp = (struct frame *)getfp();
	if (fp == NULL)
		return (NULL);

	while (fp->fr_savfp) {
		if (fp->fr_savpc == NULL)
			break;

		if (!is_even(fp->fr_savfp)) {
			fp = (struct frame *)even(fp->fr_savfp);
			if (fp == NULL)
				break;
			fp = (struct frame *)((SIGBLK *)fp)->sb_savfp;
			continue;
		}
		fp = (struct frame *)fp->fr_savfp;

		if (--i <= 0)
			break;
	}
#endif

	av = (char **)fp;
	return (av);
}

EXPORT char **
getavp()
{
	register struct frame *fp;
		char	**av;

	fp = (struct frame *)getmainfp();
	if (fp == NULL)
		return (NULL);

	av = (char **)(((char *)fp) + AV_OFFSET);	/* aus avoffset.h */
							/* -> avoffset.c  */
	return (av);
}

EXPORT char *
getav0()
{
	return (getavp()[0]);
}

#else

EXPORT char *
getav0()
{
	return ("???");
}

#endif	/* HAVE_SCANSTACK */
