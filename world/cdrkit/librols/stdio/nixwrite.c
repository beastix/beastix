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

/* @(#)nixwrite.c	1.5 04/08/08 Copyright 1986, 2001-2003 J. Schilling */
/*
 *	Non interruptable extended write
 *
 *	Copyright (c) 1986, 2001-2003 J. Schilling
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
#include <errno.h>

EXPORT int
_nixwrite(f, buf, count)
	int	f;
	void	*buf;
	int	count;
{
	register char *p = (char *)buf;
	register int ret;
	register int total = 0;
		int	oerrno = geterrno();

	while (count > 0) {
		while ((ret = write(f, p, count)) < 0) {
			if (geterrno() == EINTR) {
				/*
				 * Set back old 'errno' so we don't change the
				 * errno visible to the outside if we did
				 * not fail.
				 */
				seterrno(oerrno);
				continue;
			}
			return (ret);	/* Any other error */
		}
		if (ret == 0)		/* EOF */
			break;

		total += ret;
		count -= ret;
		p += ret;
	}
	return (total);
}
