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

/* @(#)fgetline.c	1.8 04/09/25 Copyright 1986, 1996-2003 J. Schilling */
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
 * XXX should we check if HAVE_USG_STDIO is defined and
 * XXX use something line memccpy to speed things up ???
 */

EXPORT int
fgetline(f, buf, len)
	register	FILE	*f;
			char	*buf;
	register	int	len;
{
	register int	c	= '\0';
	register char	*bp	= buf;
	register int	nl	= '\n';

	down2(f, _IOREAD, _IORW);

	for (;;) {
		if ((c = getc(f)) < 0)
			break;
		if (c == nl)
			break;
		if (--len > 0) {
			*bp++ = (char)c;
		} else {
			/*
			 * Read up to end of line
			 */
			while ((c = getc(f)) >= 0 && c != nl)
				/* LINTED */
				;
			break;
		}
	}
	*bp = '\0';
	/*
	 * If buffer is empty and we hit EOF, return EOF
	 */
	if (c < 0 && bp == buf)
		return (c);

	return (bp - buf);
}

EXPORT int
getline(buf, len)
	char	*buf;
	int	len;
{
	return (fgetline(stdin, buf, len));
}
