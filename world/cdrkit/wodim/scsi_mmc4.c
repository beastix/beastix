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

/* @(#)scsi_mmc4.c	1.1 05/05/16 Copyright 2002-2005 J. Schilling */
/*
 *	SCSI command functions for cdrecord
 *	covering MMC-4 level and above
 *
 *	Copyright (c) 2002-2005 J. Schilling
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

/* #define	DEBUG */
#include <mconfig.h>

#include <stdio.h>
#include <standard.h>
#include <stdxlib.h>
#include <unixstd.h>
#include <fctldefs.h>
#include <errno.h>
#include <strdefs.h>
#include <timedefs.h>

#include <utypes.h>
#include <btorder.h>
#include <intcvt.h>
#include <schily.h>

#include <usal/usalcmd.h>
#include <usal/scsidefs.h>
#include <usal/scsireg.h>
#include <usal/scsitransp.h>

#include "scsimmc.h"
#include "wodim.h"

 int	get_supported_cdrw_media_types(SCSI *usalp);

/*
 * Retrieve list of supported cd-rw media types (feature 0x37)
 */
int
get_supported_cdrw_media_types(SCSI *usalp)
{
	Uchar   cbuf[16];
	int	ret;
	fillbytes(cbuf, sizeof (cbuf), '\0');

	usalp->silent++;
	ret = get_configuration(usalp, (char *)cbuf, sizeof (cbuf), 0x37, 2);
	usalp->silent--;

	if (ret < 0)
		return (-1);

	if (cbuf[3] < 12)	/* Couldn't retrieve feature 0x37	*/
		return (-1);

	return (int)(cbuf[13]);
}
