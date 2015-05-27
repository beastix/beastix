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

/* @(#)breakline.c	1.10 04/09/25 Copyright 1985, 1995-2003 J. Schilling */
/*
 *	break a line pointed to by *buf into fields
 *	returns the number of tokens, the line was broken into (>= 1)
 *
 *	delim is the delimiter to break at
 *	array[0 .. found-1] point to strings from broken line
 *	array[found ... len] point to '\0'
 *	len is the size of the array
 *
 *	Copyright (c) 1985, 1995-2003 J. Schilling
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
#include <standard.h>
#include <schily.h>

#ifdef	PROTOTYPES
EXPORT int
breakline(char *buf,
		register char delim,
		register char *array[],
		register int len)
#else
EXPORT int
breakline(buf, delim, array, len)
		char	*buf;
	register char	delim;
	register char	*array[];
	register int	len;
#endif
{
	register char	*bp = buf;
	register char	*dp;
	register int	i;
	register int	found;

	for (i = 0, found = 1; i < len; i++) {
		for (dp = bp; *dp != '\0' && *dp != delim; dp++)
			/* LINTED */
			;

		array[i] = bp;
		if (*dp == delim) {
			*dp++ = '\0';
			found++;
		}
		bp = dp;
	}
	return (found);
}
