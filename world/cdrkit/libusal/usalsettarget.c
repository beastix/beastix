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

/* @(#)usalsettarget.c	1.2 04/01/14 Copyright 2000 J. Schilling */
/*
 *	usal Library
 *	set target SCSI address
 *
 *	This is the only place in libusal that is allowed to assign
 *	values to the usal address structure.
 *
 *	Copyright (c) 2000 J. Schilling
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

#include <usal/scsitransp.h>

int	usal_settarget(SCSI *usalp, int, int, int);

int
usal_settarget(SCSI *usalp, int busno, int tgt, int tlun)
{
	int fd = -1;

	if (usalp->ops != NULL)
		fd = SCGO_FILENO(usalp, busno, tgt, tlun);
	usalp->fd = fd;
	usal_scsibus(usalp) = busno;
	usal_target(usalp)  = tgt;
	usal_lun(usalp)	  = tlun;
	return (fd);
}
