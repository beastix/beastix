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

/* @(#)flag.c	2.10 05/06/12 Copyright 1986-2003 J. Schilling */
/*
 *	Copyright (c) 1986-2003 J. Schilling
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
#include <stdxlib.h>

#ifdef	DO_MYFLAG

#define	FL_INIT	10

EXPORT	int	_io_glflag;		/* global default flag */
LOCAL	int	_fl_inc = 10;		/* increment for expanding flag struct */
EXPORT	int	_fl_max = FL_INIT;	/* max fd currently in _io_myfl */
LOCAL	_io_fl	_io_smyfl[FL_INIT];	/* initial static space */
EXPORT	_io_fl	*_io_myfl = _io_smyfl;	/* init to static space */

LOCAL int _more_flags	__PR((FILE *));

LOCAL int
_more_flags(fp)
	FILE	*fp;
{
	register int	f = fileno(fp);
	register int	n = _fl_max;
	register _io_fl	*np;

	while (n <= f)
		n += _fl_inc;

	if (_io_myfl == _io_smyfl) {
		np = (_io_fl *) malloc(n * sizeof (*np));
		fillbytes(np, n * sizeof (*np), '\0');
		movebytes(_io_smyfl, np, sizeof (_io_smyfl)/sizeof (*np));
	} else {
		np = (_io_fl *) realloc(_io_myfl, n * sizeof (*np));
		if (np)
			fillbytes(&np[_fl_max], (n-_fl_max)*sizeof (*np), '\0');
	}
	if (np) {
		_io_myfl = np;
		_fl_max = n;
		return (_io_get_my_flag(fp));
	} else {
		return (_IONORAISE);
	}
}

EXPORT int
_io_get_my_flag(fp)
	register FILE	*fp;
{
	register int	f = fileno(fp);
	register _io_fl	*fl;

	if (f >= _fl_max)
		return (_more_flags(fp));

	fl = &_io_myfl[f];

	if (fl->fl_io == 0 || fl->fl_io == fp)
		return (fl->fl_flags);

	while (fl && fl->fl_io != fp)
		fl = fl->fl_next;

	if (fl == 0)
		return (0);

	return (fl->fl_flags);
}

EXPORT void
_io_set_my_flag(fp, flag)
	FILE	*fp;
	int	flag;
{
	register int	f = fileno(fp);
	register _io_fl	*fl;
	register _io_fl	*fl2;

	if (f >= _fl_max)
		(void) _more_flags(fp);

	fl = &_io_myfl[f];

	if (fl->fl_io != (FILE *)0) {
		fl2 = fl;

		while (fl && fl->fl_io != fp)
			fl = fl->fl_next;
		if (fl == 0) {
			if ((fl = (_io_fl *) malloc(sizeof (*fl))) == 0)
				return;
			fl->fl_next = fl2->fl_next;
			fl2->fl_next = fl;
		}
	}
	fl->fl_io = fp;
	fl->fl_flags = flag;
}

EXPORT void
_io_add_my_flag(fp, flag)
	FILE	*fp;
	int	flag;
{
	int	oflag = _io_get_my_flag(fp);

	oflag |= flag;

	_io_set_my_flag(fp, oflag);
}

#endif	/* DO_MYFLAG */
