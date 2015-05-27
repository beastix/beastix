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

/* @(#)fileseek.c	1.12 04/08/08 Copyright 1986, 1996-2003 J. Schilling */
/*
 *	Copyright (c) 1986, 1996-2003 J. Schilling
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

#include "schilyio.h"

/*
 * Note that because of a definition in schilyio.h we are using fseeko()/ftello()
 * instead of fseek()/ftell() if available.
 */

static	char	_seekerr[]	= "file_seek_err";

EXPORT int
fileseek(f, pos)
	register FILE	*f;
	off_t	pos;
{
	int	ret;

	down(f);
	ret = fseek(f, pos, SEEK_SET);
	if (ret < 0 && !(my_flag(f) & _IONORAISE))
		raisecond(_seekerr, 0L);
	return (ret);
}
