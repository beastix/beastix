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

/* @(#)getnum.c	1.2 04/03/02 Copyright 1984-2002, 2004 J. Schilling */
/*
 *	Number conversion routines to implement 'dd' like options.
 *
 *	Copyright (c) 1984-2002, 2004 J. Schilling
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
#include <utypes.h>
#include <schily.h>

static	Llong	number(char *arg, int *retp);
int	getnum(char *arg, long *valp);
int	getllnum(char *arg, Llong *lvalp);

static Llong
number(register char *arg, int *retp)
{
	Llong	val	= 0;

	if (*retp != 1)
		return (val);
	if (*arg == '\0') {
		*retp = -1;
	} else if (*(arg = astoll(arg, &val))) {
		if (*arg == 'p' || *arg == 'P') {
			val *= (1024*1024);
			val *= (1024*1024*1024);
			arg++;

		} else if (*arg == 't' || *arg == 'T') {
			val *= (1024*1024);
			val *= (1024*1024);
			arg++;

		} else if (*arg == 'g' || *arg == 'G') {
			val *= (1024*1024*1024);
			arg++;

		} else if (*arg == 'm' || *arg == 'M') {
			val *= (1024*1024);
			arg++;

		} else if (*arg == 'f' || *arg == 'F') {
			val *= 2352;
			arg++;

		} else if (*arg == 's' || *arg == 'S') {
			val *= 2048;
			arg++;

		} else if (*arg == 'k' || *arg == 'K') {
			val *= 1024;
			arg++;

		} else if (*arg == 'b' || *arg == 'B') {
			val *= 512;
			arg++;

		} else if (*arg == 'w' || *arg == 'W') {
			val *= 2;
			arg++;
		}
		if (*arg == '*' || *arg == 'x')
			val *= number(++arg, retp);
		else if (*arg != '\0')
			*retp = -1;
	}
	return (val);
}

int
getnum(char *arg, long *valp)
{
	Llong	llval;
	int	ret = 1;

	llval = number(arg, &ret);
	*valp = llval;
	if (*valp != llval) {
		errmsgno(EX_BAD,
			"Value %lld is too large for data type 'long'.\n",
									llval);
		ret = -1;
	}
	return (ret);
}

int
getllnum(char *arg, Llong *lvalp)
{
	int	ret = 1;

	*lvalp = number(arg, &ret);
	return (ret);
}
