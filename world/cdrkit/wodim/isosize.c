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

/* @(#)isosize.c	1.9 04/03/02 Copyright 1996, 2001-2004 J. Schilling */
/*
 *	Copyright (c) 1996, 2001-2004 J. Schilling
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
#include <statdefs.h>
#include <unixstd.h>
#include <standard.h>
#include <utypes.h>
#include <intcvt.h>

#include "iso9660.h"
#include "wodim.h"	/* to verify isosize() prototype */

Llong	isosize(int f);

Llong
isosize(int f)
{
	struct iso9660_voldesc		vd;
	struct iso9660_pr_voldesc	*vp;
	Llong				isize;
	struct stat			sb;
	mode_t				mode;

	/*
	 * First check if a bad guy tries to call isosize()
	 * with an unappropriate file descriptor.
	 * return -1 in this case.
	 */
	if (isatty(f))
		return ((Llong)-1);
	if (fstat(f, &sb) < 0)
		return ((Llong)-1);
	mode = sb.st_mode & S_IFMT;
	if (!S_ISREG(mode) && !S_ISBLK(mode) && !S_ISCHR(mode))
		return ((Llong)-1);

	if (lseek(f, (off_t)(16L * 2048L), SEEK_SET) == -1)
		return ((Llong)-1);

	vp = (struct iso9660_pr_voldesc *) &vd;

	do {
		read(f, &vd, sizeof (vd));
		if (GET_UBYTE(vd.vd_type) == VD_PRIMARY)
			break;

	} while (GET_UBYTE(vd.vd_type) != VD_TERM);

	lseek(f, (off_t)0L, SEEK_SET);

	if (GET_UBYTE(vd.vd_type) != VD_PRIMARY)
		return (-1L);

	isize = (Llong)GET_BINT(vp->vd_volume_space_size);
	isize *= GET_BSHORT(vp->vd_lbsize);
	return (isize);
}
