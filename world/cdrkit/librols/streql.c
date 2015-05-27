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

/* @(#)streql.c	1.7 03/06/15 Copyright 1985, 1995-2003 J. Schilling */
/*
 *	Check if two strings are equal
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

#include <standard.h>
#include <schily.h>

EXPORT int
streql(a, b)
	const char	*a;
	const char	*b;
{
	register const char	*s1 = a;
	register const char	*s2 = b;

	if (s1 == NULL || s2 ==  NULL)
		return (FALSE);

	while (*s1 == *s2++)
		if (*s1++ == '\0')
			return (TRUE);

	return (FALSE);
}
